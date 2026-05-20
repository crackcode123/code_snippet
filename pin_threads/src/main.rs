use std::{thread, time::{Duration, Instant}, fs, collections::BTreeSet, sync::{Arc, Mutex}};
use rand::Rng;

fn num_cpus_online() -> usize {
    fs::read_to_string("/sys/devices/system/cpu/online").unwrap().
            trim().split(',').flat_map(|r| {
            println!("inside {}",r);
            let (a, b) = r.split_once('-').unwrap_or((r, r));
            a.parse::<usize>().unwrap()..=b.parse().unwrap()
        }).count()
}

fn physical_cores() -> Vec<usize> {
    let mut seen = BTreeSet::new();
    let mut out = Vec::new();
    for cpu in 0..num_cpus_online() {
        let s = fs::read_to_string(format!(
            "/sys/devices/system/cpu/cpu{}/topology/thread_siblings_list", cpu)).unwrap();
        if seen.insert(s.trim().to_string()) { out.push(cpu); }
    }
    out
}

#[derive(Clone, Copy)]
struct Event { tid: u16, cpu: u16, at_ms: u64 }

fn main() {
    let cpus = physical_cores();
    let log: Arc<Mutex<Vec<Event>>> = Arc::new(Mutex::new(Vec::with_capacity(1 << 20)));
    let start = Instant::now();
    println!("pinning {} physical cores: {:?}", cpus.len(), cpus);

    for (i, cpu) in cpus.iter().copied().enumerate() {
        let log = log.clone();
        thread::spawn(move || {
            core_affinity::set_for_current(core_affinity::CoreId { id: cpu });
            thread::sleep(Duration::from_millis((i as u64) * 40));
            let mut rng = rand::thread_rng();
            loop {
                thread::sleep(Duration::from_millis(rng.gen_range(10..=1000)));
                let ev = Event { tid: i as u16, cpu: cpu as u16, at_ms: start.elapsed().as_millis() as u64 };
                log.lock().unwrap().push(ev);
            }
        });
    }

    loop {
        thread::sleep(Duration::from_secs(5));
        let len = log.lock().unwrap().len();
        println!("[{:>4}s] events buffered: {}", start.elapsed().as_secs(), len);
    }
}
