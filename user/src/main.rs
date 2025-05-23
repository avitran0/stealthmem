use std::{fs::OpenOptions, os::fd::AsRawFd};

use clap::Parser;
use nix::{ioctl_readwrite, libc};

fn maybe_hex(s: &str) -> Result<u64, std::num::ParseIntError> {
    const HEX_PREFIX: &str = "0x";
    const HEX_PREFIX_UPPER: &str = "0X";
    const HEX_PREFIX_LEN: usize = HEX_PREFIX.len();

    if s.starts_with(HEX_PREFIX) || s.starts_with(HEX_PREFIX_UPPER) {
        u64::from_str_radix(&s[HEX_PREFIX_LEN..], 16)
    } else {
        s.parse::<u64>()
    }
}

#[derive(Parser)]
struct Args {
    pid: i32,
    #[arg(value_parser=maybe_hex)]
    address: u64,
    size: usize,
}

#[repr(C)]
struct MemoryParams {
    pid: libc::pid_t,
    addr: libc::c_ulong,
    size: libc::size_t,
    buf: *mut libc::c_void,
}

impl MemoryParams {
    pub fn from_args(args: &Args, buf: *mut libc::c_void) -> Self {
        Self {
            pid: args.pid,
            addr: args.address,
            size: args.size,
            buf,
        }
    }
}

const IOCTL_MAGIC_READ: u8 = 0xBC;
const IOCTL_MAGIC_WRITE: u8 = 0xBD;

ioctl_readwrite!(ioctl_read_mem, IOCTL_MAGIC_READ, 1, MemoryParams);
ioctl_readwrite!(ioctl_write_mem, IOCTL_MAGIC_WRITE, 1, MemoryParams);

fn main() {
    let args = Args::parse();

    let mut buffer = vec![0u8; args.size];

    let mut params = MemoryParams::from_args(&args, buffer.as_mut_ptr() as *mut libc::c_void);

    let Ok(file) = OpenOptions::new()
        .write(true)
        .read(true)
        .open("/dev/stealthmem")
    else {
        eprintln!("could not open device file");
        return;
    };

    let Ok(bytes_read) = (unsafe { ioctl_read_mem(file.as_raw_fd(), &mut params) }) else {
        eprintln!("failed to read memory");
        return;
    };

    println!(
        "read {bytes_read} from pid {} at {:x}",
        params.pid, params.addr
    );
    println!("value: {:?}", buffer);
}
