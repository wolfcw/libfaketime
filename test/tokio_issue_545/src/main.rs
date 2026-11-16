use std::time::Duration;

fn std_thread_sleep(duration: Duration) {
    println!("[std_thread_sleep] Starting sleep {duration:?}...");
    std::thread::sleep(duration);
    println!("[std_thread_sleep] Done sleep.");
}

fn tokio_time_sleep(duration: Duration) {
    println!("[tokio_time_sleep] Starting sleep {duration:?}...");
    let runtime = tokio::runtime::Runtime::new().expect("failed to create Tokio runtime");
    runtime.block_on(async { tokio::time::sleep(duration).await });
    println!("[tokio_time_sleep] Done sleep.");
}

fn main() {
    let mut arguments = std::env::args().skip(1);
    let method = arguments.next().unwrap_or_else(|| "--help".to_owned());
    let seconds = arguments
        .next()
        .and_then(|value| value.parse::<u64>().ok())
        .unwrap_or(1);
    let duration = Duration::from_secs(seconds);

    match method.as_str() {
        "std_thread_sleep" => std_thread_sleep(duration),
        "tokio_time_sleep" => tokio_time_sleep(duration),
        _ => {
            eprintln!("usage: libfaketime-tokio-issue-545 [std_thread_sleep|tokio_time_sleep] [seconds]");
            std::process::exit(2);
        }
    }
}
