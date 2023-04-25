use std::{thread, time};
use std::io::{self, Write};
mod fib;

fn main() {
	println!("[Rust (dynamic) main]");
	for i in 0..3 {
		if i != 0 {
			thread::sleep(time::Duration::from_millis(10000));
		}
		unsafe {
			println!("fib({}) = {}", i, fib::fib(i) );
			io::stdout().flush().expect("Unable to flush stdout");
			fib::printfib(21 + i);
		}
	}
}