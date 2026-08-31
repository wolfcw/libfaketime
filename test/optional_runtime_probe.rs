use std::time::{SystemTime, UNIX_EPOCH};

fn main() {
    println!("{}", SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_secs());
}
