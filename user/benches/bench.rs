use std::{fs::OpenOptions, os::fd::AsRawFd};

use criterion::{Criterion, criterion_group, criterion_main};
use nix::{ioctl_readwrite, libc};

#[repr(C)]
pub struct MemoryParams {
    pid: libc::pid_t,
    addr: libc::c_ulong,
    size: libc::size_t,
    buf: *mut libc::c_void,
}

const IOCTL_MAGIC_READ: u8 = 0xBC;
const IOCTL_MAGIC_WRITE: u8 = 0xBD;

ioctl_readwrite!(ioctl_read_mem, IOCTL_MAGIC_READ, 1, MemoryParams);
ioctl_readwrite!(ioctl_write_mem, IOCTL_MAGIC_WRITE, 1, MemoryParams);

const fn u64() -> [u8; 8] {
    0x1234567890abcdefu64.to_ne_bytes()
}

fn bench_syscall(c: &mut Criterion) {
    let value = u64();
    let mut buffer = vec![0u8; std::mem::size_of::<u64>()];

    let mut params = MemoryParams {
        pid: std::process::id() as i32,
        addr: value.as_ptr() as libc::c_ulong,
        size: std::mem::size_of::<u64>(),
        buf: buffer.as_mut_ptr() as *mut libc::c_void,
    };

    let Ok(file) = OpenOptions::new()
        .write(true)
        .read(true)
        .open("/dev/stealthmem")
    else {
        eprintln!("could not open device file");
        return;
    };

    c.bench_function("syscall", |b| {
        b.iter(|| {
            let _ = unsafe { ioctl_read_mem(file.as_raw_fd(), &mut params) };
        });
    });
}

criterion_group!(benches, bench_syscall);
criterion_main!(benches);
