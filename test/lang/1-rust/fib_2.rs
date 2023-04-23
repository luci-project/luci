use std::os::raw::c_ushort;
use std::os::raw::c_long;
use std::io::{self, Write};

#[no_mangle]
pub static version : c_ushort = 2;

fn fibalgo (value : c_long) -> c_long {
	let mut l = 0;
	let mut p = 1;
	let mut n = value;
	for _i in 1..value {
		n = l + p;
		l = p;
		p = n;
	}
	return n;
}

#[no_mangle]
pub fn fib (value : c_long) -> c_long {
	return fibalgo(value)
}

#[no_mangle]
pub fn printfib (value : c_long) {
	println!("[Rust Fibonacci Library v{}] fib({}) = {}", version, value, fibalgo(value));
	io::stdout().flush().expect("Unable to flush stdout");
}


