use std::time::{Duration, SystemTime, UNIX_EPOCH};

#[tokio::main(flavor = "current_thread")]
async fn main() {
    tokio::time::sleep(Duration::from_millis(20)).await;
    println!("{}", SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_secs());
}
