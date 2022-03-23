const CMD_SOF = '>';
const CMD_DELIM_HEX = '#';
const CMD_DELIM_BIN = '%';
const CMD_VERS = 'V';
const CMD_SEND = 'S';
const CMD_DATA = 'D';
const CMD_AUTH = 'U';
const CMD_STRAP = 'O';
const CMD_OTPD = 'P';
const CMD_OTPR = 'R';
const CMD_OTPC = 'M';
const CMD_OTP_REGIONS = 'G';
const CMD_CONT = 'C';
const CMD_SJTAG_RD = 'Q';
const CMD_SJTAG_WR = 'A';
const CMD_ACK = 'a';
const CMD_NACK = 'n';
const CMD_TRACE = 'T';

function fmtHex(arg)
{
    var h = (arg >>> 0).toString(16).padStart(8, "0");
    return h;
}

function fmtReq(cmdName, arg, data)
{
    var buf = cmdName + ',' + fmtHex(arg);
    if (data) {
	throw "No data support yet";
    } else {
	buf += ',' + fmtHex(0) + CMD_DELIM_HEX;
    }
    buf = CMD_SOF + buf + fmtHex(CRC32C.str(buf));
    return buf;
}

function validResponse(r)
{
    var m = r.match(/^>(\w),([0-9a-f]{8}),([0-9a-f]{8})(#|%)(.+)$/);
    //console.log(m);
    return m;
}

function decodeData(type, data)
{
    if (type == '#') {		// hex encoded
	var result = "";
	while (data.length >= 2) {
            result += String.fromCharCode(parseInt(data.substring(0, 2), 16));
            data = data.substring(2, data.length);
	}
	return result;
    } else if (type == '%') {	// Raw binary
	return data;
    } else {
	throw "Invalid data type" + type;
    }
}

function parseResponse(r)
{
    var m = r.match(/^>(\w),([0-9a-f]{8}),([0-9a-f]{8})(#|%)(.+)$/);
    //console.log(m);
    if (m) {
	var tail = m[5];
	var tailcrc = tail.substr(-8).toLowerCase();
	var resp = {
	    'command': m[1],
	    'arg':     parseInt(m[2], 16),
	    'length':  parseInt(m[3], 16),
	};
	// Zoom into CRC32 data
	var data = r.substr(1, r.length - 8 - 1);
	crc32 = fmtHex(CRC32C.str(data)).toLowerCase();
	//console.log("Calc: " + crc32 + " - Rcv: " + tailcrc);
	if (crc32 == tailcrc) {
	    resp["crc"] = crc32;
	    resp["data"] = decodeData(m[4], tail.substr(0, tail.length - 8));
	}
	return resp["crc"] && (resp["length"] == resp["data"].length) ?
	    resp : nil;
    }
    return nil;
}

async function readTail(reader, decoder, total)
{
    var buf = "";
    while (buf.length < total) {
	try {
	    const { value, done } = await reader.read();
	    if (value) {
		var chunk = decoder.decode(value);
		buf += chunk;
	    }
	    if (done) {
		console.log("DONE");
		break;
	    }
	} catch (error) {
	    // TODO: Handle non-fatal read error.
	    console.log("error" + error);
	}
    }
    return buf;
}

async function getResponse(port)
{
    var resp = "";
    if (port.readable) {
	const decoder = new TextDecoder();
	const reader = port.readable.getReader();
	//console.log("Enter reader loop");
	var rmatch;
	// Read until we can see the request length
	while (!(rmatch = validResponse(resp))) {
	    try {
		const { value, done } = await reader.read();
		if (value) {
		    resp += decoder.decode(value);
		}
		if (done) {
		    console.log("DONE");
		    break;
		}
	    } catch (error) {
		// TODO: Handle non-fatal read error.
		console.log("error");
	    }
	}
	// Now read the remainder data/CRC to complete the response
	if (rmatch) {
	    var total = parseInt(rmatch[3], 16);
	    var tail = rmatch[5];
	    var missing = (total * 2) + 8 - tail.length;
	    var rest = await readTail(reader, decoder, missing);
	    resp += rest;
	}
	//console.log("Exit reader loop");
	reader.releaseLock();
    }
    return resp;
}

async function sendRequest(port, buf)
{
    const encoder = new TextEncoder();
    const writer = port.writable.getWriter();
    await writer.write(encoder.encode(buf));
    writer.releaseLock();
}

function startSerial()
{
    document.querySelector('button').addEventListener('click', async () => {
	const port = await navigator.serial.requestPort();
	await port.open({ baudRate: 115200});

	await sendRequest(port, fmtReq(CMD_VERS, 0));
	var response = await getResponse(port);
	//console.log("Response: " + response);
	var rspStruct = parseResponse(response);
	console.log("Response: %o", rspStruct);
    });
}
