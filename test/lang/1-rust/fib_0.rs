use std::os::raw::c_ushort;
use std::os::raw::c_long;
use std::io::{self, Write};

#[no_mangle]
pub static version : c_ushort = 0;

fn fibalgo (value : c_long) -> c_long {
	if value < 2 {
		return value
	} else {
		return fibalgo(value - 1) + fibalgo(value - 2);
	}
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
