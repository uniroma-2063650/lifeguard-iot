import { SerialPort } from "serialport";
import * as readline from "node:readline/promises";

const path = process.argv[2] ?? "/dev/tty.usbserial-10";
const baudRate = Number(process.argv[3] ?? 19600);

console.log(`Reading from ${path} at ${baudRate} Hz`);

const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
});

const port = new SerialPort(
    {
        path,
        baudRate,
    },
    (error) => {
        if (!error) return;
        console.error("Couldn't open serial port:");
        console.error(error.message);
        process.exit(1);
    }
);

let had_newline = { value: false };
port.on("data", (data) => {
    if (had_newline.value) {
        const now = new Date();
        process.stdout.write(`${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}:${now.getSeconds().toString().padStart(2, '0')}.${now.getMilliseconds().toString().padStart(3, '0')}: `);
    }
    process.stdout.write(data.toString());
    had_newline.value = data.toString().charAt(data.length - 1) == "\n";
});

rl.on("line", (data) => {
    port.write(data, "utf8", (error) => {
        if (!error) return;
        console.error("Couldn't write to serial port:");
        console.error(error.message);
        process.exit(1);
    });
});

port.on("error", (error) => {
    console.error("Serial port error:");
    console.error(error.message);
    process.exit(1);
});

