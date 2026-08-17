#!/usr/bin/env run-cargo-script
//! ```cargo
//! [dependencies]
//! time = "0.1.25"
//! walkdir = "1"
//! clap = "~2.26"
//! regex = "0.2"
//! wait-timeout = "0.1.5"
//! ```
/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 2018, The OpenClonk Team and contributors
 *
 * Distributed under the terms of the ISC license; see accompanying file
 * "COPYING" for details.
 *
 * "Clonk" is a registered trademark of Matthes Bender, used with permission.
 * See accompanying file "TRADEMARK" for details.
 *
 * To redistribute this file separately, substitute the full license texts
 * for the above references.
 */

extern crate walkdir;
extern crate regex;
extern crate wait_timeout;
#[macro_use]
extern crate clap;

use std::path::Path;
use std::error::Error;
use std::io::prelude::*;
use std::process::{Command, Stdio};
use walkdir::WalkDir;
//use clap::clap_app;
use std::thread;
use std::sync::mpsc;
use std::io::BufReader;
use std::time::{Instant, Duration};
use regex::Regex;
use wait_timeout::ChildExt;

fn main() {
	let matches = clap_app!(myapp =>
		(version: "0.4")
		(author: "Julius Michaelis <jcoc@liftm.de>")
		(about: "Test-run all scenarios, grep for script errors")
		(@arg planet: -p --planet +takes_value "Path to planet directory containing game data")
		(@arg openclonk: -e --openclonk +takes_value "Path to headless executable")
		(@arg expect: -x --expect +takes_value "Pinned counts to check against; exit 1 on any deviation")
		(@arg verbose: -v --verbose ... "Verbosity")
	).get_matches();
	let oc = matches.value_of("openclonk").unwrap_or("./openclonk-server");
	let planet = matches.value_of("planet").unwrap_or("./planet");
	let verbosity = matches.occurrences_of("verbose");

	let mut results : Vec<(String, Option<Counts>)> = Vec::new();
	for entry in WalkDir::new(planet).into_iter()
		.filter_map(|e| e.ok())
		.filter(|e| e.file_name().to_str()
			.map_or(false, |s| s.ends_with(".ocs")))
	{
		let path = entry.path();
		let counts = test(path, oc, verbosity);
		results.push((relative_to(path, planet), counts));
	}

	match matches.value_of("expect") {
		None => {
			println!("{} scenarios run. No --expect given, so nothing was checked.", results.len());
		},
		Some(expectations) => {
			if !check(&results, expectations) {
				std::process::exit(1);
			}
		},
	};
}

/// The engine writes "1 warning" but "0 warnings"; match it.
fn plural(n : u32) -> &'static str {
	if n == 1 { "" } else { "s" }
}

/// Warnings and errors reported by `C4AulScriptEngine linked`.
#[derive(PartialEq, Eq, Clone, Copy)]
struct Counts {
	warnings : u32,
	errors : u32,
}

/// Scenario path as it is written in the expectations file: relative to the
/// planet directory, so the file does not depend on how this was invoked.
fn relative_to(scen : &Path, planet : &str) -> String {
	let full = scen.to_string_lossy().replace('\\', "/");
	let base = planet.replace('\\', "/");
	let base = base.trim_end_matches('/');
	if full.starts_with(base) {
		full[base.len()..].trim_start_matches('/').to_string()
	} else {
		full
	}
}

/// Read `<warnings> <errors> <path>` lines, skipping blanks and # comments.
fn read_expectations(path : &str) -> Vec<(String, Counts)> {
	let mut file = match std::fs::File::open(path) {
		Err(why) => panic!("cannot read {}: {}", path, why.description()),
		Ok(file) => file,
	};
	let mut text = String::new();
	file.read_to_string(&mut text).expect("cannot read expectations");

	let mut out = Vec::new();
	for line in text.lines() {
		let line = line.trim();
		if line.is_empty() || line.starts_with('#') {
			continue;
		}
		let fields : Vec<&str> = line.splitn(3, char::is_whitespace)
			.map(|f| f.trim())
			.filter(|f| !f.is_empty())
			.collect();
		if fields.len() != 3 {
			panic!("malformed line in {}: {}", path, line);
		}
		let counts = Counts {
			warnings: fields[0].parse().expect("warning count is not a number"),
			errors: fields[1].parse().expect("error count is not a number"),
		};
		out.push((fields[2].to_string(), counts));
	}
	out
}

/// Compare what was measured against what is pinned. Every deviation is a
/// failure, in both directions: a count that grew is a regression, and a count
/// that shrank means someone fixed something and has to bring the pin down
/// with it, or the pin quietly stops protecting anything.
fn check(results : &[(String, Option<Counts>)], expectations : &str) -> bool {
	let expected = read_expectations(expectations);
	let mut ok = true;

	for &(ref scen, counts) in results {
		match expected.iter().find(|&&(ref e, _)| e == scen) {
			None => {
				println!("FAIL {}: not in {}. Add it with the counts it reports.",
					scen, expectations);
				ok = false;
			},
			Some(&(_, want)) => match counts {
				None => {
					println!("FAIL {}: reported nothing. The engine did not get far \
enough to link its scripts -- look for a missing object group.", scen);
					ok = false;
				},
				Some(got) => if got != want {
					println!("FAIL {}: {} warning{}, {} error{}; pinned at {} and {}.{}",
						scen,
						got.warnings, plural(got.warnings),
						got.errors, plural(got.errors),
						want.warnings, want.errors,
						if got.warnings <= want.warnings && got.errors <= want.errors {
							" Something was fixed -- lower the pin in the same commit."
						} else {
							""
						});
					ok = false;
				},
			},
		};
	}

	for &(ref scen, _) in expected.iter() {
		if !results.iter().any(|&(ref r, _)| r == scen) {
			println!("FAIL {}: pinned in {} but no such scenario was found.",
				scen, expectations);
			ok = false;
		}
	}

	if ok {
		println!("{} scenarios, all matching {}.", results.len(), expectations);
	}
	ok
}

fn test(scen : &Path, oc : &str, verbosity : u64) -> Option<Counts> {

	if verbosity >= 1 {
		println!("Testing {}", scen.display());
	}

	let start = Instant::now();
	let test_dur = Duration::from_secs(450);
	let shutdown_to = Duration::from_secs(15);
	let test_deadline = start + test_dur - shutdown_to;

	// [07:45:04] C4AulScriptEngine linked - 86722 lines, 0 warnings, 0 errors
	let status_re = Regex::new(r"^\[\d\d:\d\d:\d\d\] C4AulScriptEngine linked - (.*)\n$").unwrap();
	// ... of which this picks out the two numbers that are worth pinning.
	// Singular and plural both occur: "1 warning" but "0 warnings".
	let counts_re = Regex::new(r"(\d+) warnings?, (\d+) errors?").unwrap();
	// [07:45:08] Game started.
	let load_done_re = Regex::new(r"^\[\d\d:\d\d:\d\d\] (Game started|Game cleared).\n$").unwrap();

	let mut counts = None;

	let mut process = match Command::new(oc)
        .arg("--language=US")
		.arg(scen)
		.arg("Test.ocp")
		.stdin(Stdio::piped())
		.stdout(Stdio::piped())
		.stderr(Stdio::null())
		.spawn() 
	{
		Err(why) => panic!("couldn't spawn oc: {}", why.description()),
		Ok(process) => process,
	};

	let ocout = cont_read(process.stdout.take().unwrap(), verbosity >= 3);

	// Look for error counts
	loop {
		match ocout.recv_timeout(dur_till_then(test_deadline)) {
			Err(mpsc::RecvTimeoutError::Timeout) => {
				println!("Timeout: Could not finish test of {}, shutting down", scen.display());
				break;
			},
			Err(std::sync::mpsc::RecvTimeoutError::Disconnected) => {
				println!("Error testing {}, OC seems to have died.", scen.display());
				break;
			},
			Ok(msg) => {
				match status_re.captures(&msg) {
					None => {},
					Some(caps) => {
						let status = caps.get(1).unwrap().as_str();
						println!("{}: {}", scen.display(), status);
						counts = counts_re.captures(status).map(|c| Counts {
							warnings: c.get(1).unwrap().as_str().parse().unwrap(),
							errors: c.get(2).unwrap().as_str().parse().unwrap(),
						});
					},
				};
				match load_done_re.find(&msg) {
					None => {},
					Some(_) => {
						break;
					},
				};
			},
		};
	}

	let finish_deadline = Instant::now() + shutdown_to;
	if verbosity >= 1 {
		println!("shutting down");
	}
	// Shut down
	drop(process.stdin.take());
	loop {
		match ocout.recv_timeout(dur_till_then(finish_deadline)) {
			Err(mpsc::RecvTimeoutError::Timeout) => {
				println!("Error testing {}: Did not shut down within deadline. Killing.", scen.display());
				process.kill().expect("Failure to kill OC.");
				break;
			},
			Err(std::sync::mpsc::RecvTimeoutError::Disconnected) => {
				break; // That should be a clean exit
			},
			Ok(_) => {},
		};
	}

	let status_code = match process.wait_timeout(dur_till_then(finish_deadline)).unwrap() {
		Some(status) => status.code(),
		None => {
            if verbosity >= 1 {
                println!("Process not dead yet, killing more.");
            }
			process.kill().unwrap();
			process.wait().unwrap().code()
		}
	};
	if verbosity >= 1 {
		println!("exited with: {}", status_code.map(|c| c.to_string()).as_ref().map(|x| &**x).unwrap_or("[no code]"))
	}

	counts
}

fn cont_read(pipe : std::process::ChildStdout, print : bool) -> std::sync::mpsc::Receiver<String> {
	// Rust is unfinished… :/ (Or I'm too stupid to find a way to do it without an extra thread.)
	// Either way, I need this workaround.

	let (tx, rx) = mpsc::channel();

	thread::spawn(move || {
		let mut reader = BufReader::new(pipe);
		loop {
			let mut buffer = String::new();
			let num_bytes = reader.read_line(&mut buffer).unwrap();
			if num_bytes < 1 {
				return
			}
			if print {
				print!(">> {}", buffer);
			}
			match tx.send(buffer) {
				Err(_) => return,
				Ok(_) => {},
			};
		}
	});

	return rx;
}

fn dur_till_then(t : std::time::Instant) -> std::time::Duration {
		let now = Instant::now();
		if t < now {
			return Duration::from_secs(0);
		}
        return t - now;
}
