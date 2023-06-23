const CMD_SOF = '>';
const CMD_DELIM_HEX = '#';
const CMD_DELIM_BIN = '%';
const CMD_VERS = 'V';
const CMD_SEND = 'S';
const CMD_UNZIP = 'Z';
const CMD_DATA = 'D';
const CMD_AUTH = 'U';
const CMD_OTPD = 'P';
const CMD_OTPR = 'R';
const CMD_OTP_REGIONS = 'G';
const CMD_SJTAG_RD = 'Q';
const CMD_SJTAG_WR = 'A';
const CMD_ACK = 'a';
const CMD_NACK = 'n';
const CMD_BL2U_WRITE = 'W';
const CMD_BL2U_IMAGE = 'I';
const CMD_BL2U_BIND ='B';
const CMD_BL2U_OTP_READ ='L';
const CMD_BL2U_RESET ='e';
const CMD_BL2U_DDR_CFG_SET = 'C';
const CMD_BL2U_DDR_CFG_GET = 'c';
const CMD_BL2U_DDR_TEST = 'T';

let cur_stage = "connect";	// Initial "tab"
let tracing = false;
let port_reader;

const otp_max_offset = 8192;
const otp_max_read = 256;

const otp_flag_rnd = 0x01;
const otp_flag_set = 0x02;

const otp_fields = [
    //{"name": 'OTP_PRG',			"offset": 0, "size": 4, },
    {"name": 'FEAT_DIS',		"offset": 4, "size": 1, },
    {"name": 'PARTID',			"offset": 5, "size": 2, },
    {"name": 'TST_TRK',			"offset": 7, "size": 1, },
    {"name": 'SERIAL_NUMBER',		"offset": 8, "size": 8, },
    {"name": 'SECURE_JTAG',		"offset": 16, "size": 4, },
    {"name": 'WAFER_TRK',		"offset": 20, "size": 7, },
    {"name": 'JTAG_UUID',		"offset": 32, "size": 10, },
    {"name": 'TRIM',			"offset": 48, "size": 8, },
    {"name": 'PROTECT_OTP_WRITE',	"offset": 64, "size": 4, },
    {"name": 'PROTECT_REGION_ADDR',	"offset": 68, "size": 32, },
    {"name": 'PCIE_FLAGS',		"offset": 100, "size": 4, },
    {"name": 'PCIE_DEV',		"offset": 104, "size": 4, },
    {"name": 'PCIE_ID',			"offset": 108, "size": 8, },
    {"name": 'PCIE_BARS',		"offset": 116, "size": 40, },
    {"name": 'Root of Trust (ROTPK)',	"offset": 256, "size": 32,	"use": otp_flag_set, },
    {"name": 'Private key (HUK)',	"offset": 288, "size": 32,	"use": otp_flag_rnd, },
    {"name": 'Endorsement key (EK)',	"offset": 320, "size": 32,	"use": otp_flag_set, },
    {"name": 'Shared key (SSK)',	"offset": 352, "size": 32,	"use": otp_flag_set, },
    {"name": 'SJTAG Key',		"offset": 384, "size": 32,	"use": otp_flag_set, },
    {"name": 'STRAP disable mask',	"offset": 420, "size": 2, 	"use": otp_flag_set, },
    {"name": 'TBBR_NTNVCT',		"offset": 512, "size": 32, },
    {"name": 'TBBR_TNVCT',		"offset": 544, "size": 32, },
]

const platforms = {
    "00000000": {		//  LAN966X B0 BL1 responds *without* chipid
	"name":		"LAN966X B0",
	"bl1_binary":	false,
	"bl2u":		lan966x_b0_app,
	"ddr_cfg":	ddr_cfg_lan966x,
	"ddr_regs":	ddr_regs_lan966x,
    },
    "19660445": {		// LAN966X B0 BL2U responds with chipid
	"name":		"LAN966X B0",
	"bl1_binary":	false,
	"bl2u":		lan966x_b0_app,
	"ddr_cfg":	ddr_cfg_lan966x,
	"ddr_regs":	ddr_regs_lan966x,
    },
    "0969a445": {
	"name":		"LAN969X A0",
	"bl1_binary":	true,
	"bl2u":		lan969x_a0_app,
	"ddr_cfg":	ddr_cfg_lan969x,
	"ddr_regs":	ddr_regs_lan969x,
    },
};

function validResponse(r)
{
    var m = r.match(/>(\w),([0-9a-f]{8}),([0-9a-f]{8})(#|%)(.+)/i);
    //console.log(m);
    return m;
}

class BootstrapRequestTransformer {
    constructor() {
	// A container for holding stream data until a new line.
	this.chunks = "";
	this.sync = false;	// Received '>' already
	// Length of request "fixed" parts
	this.fixedlen = ">X,00000000,00000000#00000000".length;
    }

    transform(chunk, controller) {
	// Append new chunks to existing chunks.
	this.chunks += chunk;

	if (!this.sync) {
	    var sync_ix = chunk.indexOf('>');
	    var skipped = "";
	    if (sync_ix != -1) {
		skipped = chunk.substr(0, sync_ix - 1);
		this.chunks = chunk.substr(sync_ix);
		this.sync = true;
		if (tracing)
		    console.log("Sync: %s", this.chunks);
	    } else {
		skipped = chunk;
	    }
	    if (skipped.length)
		console.log("Skipped: %s", skipped);
	}

	if (this.sync) {
	    if (tracing)
		console.log("Xform: %s -> %s", chunk, this.chunks);
	    // Enough data to have a real request?
	    if (this.chunks.length >= this.fixedlen) {
		var rmatch = validResponse(this.chunks);
		if (rmatch) {
		    var datalen = parseInt(rmatch[3], 16);
		    var reqlen = (datalen * 2) + this.fixedlen;
		    if (tracing)
			console.log("valid: datalen %d, reqlen %d", datalen, reqlen);
		    if (this.chunks.length >= reqlen) {
			controller.enqueue(this.chunks.substr(0, reqlen));
			this.sync = false;
			this.chunks = this.chunks.substr(reqlen);
		    } else {
			if (tracing)
			    console.log("short: datalen %d, reqlen %d, has %d", datalen,
					reqlen, this.chunks.length);
		    }
		}
	    }
	}
    }

    flush(controller) {
	// When the stream is closed, flush any remaining chunks out.
	this.chunks = "";
	this.sync = false;
    }
}

function getPlatform(value)
{
    return platforms[value];
}

function fmtHex(arg)
{
    var h = (arg >>> 0).toString(16).padStart(8, "0");
    return h;
}

function ntohl(arg)
{
    var ret = "";
    for (var i = 24; i >= 0; i -= 8)
	ret += String.fromCharCode(0xff & (arg >> i));
    return ret;
}

function hexString2Bin(s)
{
    var bytes = s.split(":");
    var str = "";
    for (var i = 0; i < bytes.length; i++) {
	var b = bytes[i];
	if (b.match(/^[0-9a-f]{2}$/i)) {
	    str += String.fromCharCode(parseInt(b, 16));
	} else {
	    throw "Illegal data: " + b;
	}
    }
    return str;
}

function encodeString2Array(buf)
{
    var bArr = new Uint8Array(buf.length);
    for (var i = 0; i < buf.length; i++) {
	bArr[i] = buf.charCodeAt(i);
    }
    return bArr;
}

function fmtReq(cmdName, arg, data, binary)
{
    var buf = cmdName + ',' + fmtHex(arg);
    if (data) {
	let encodedData = "";
	buf += ',' + fmtHex(data.length);
	if (binary) {
	    buf += CMD_DELIM_BIN;
	    for (var i = 0; i < data.length; i++) {
		encodedData += String.fromCharCode(data.charCodeAt(i));
	    }
	} else {
	    buf += CMD_DELIM_HEX;
	    for (var i = 0; i < data.length; i++) {
		encodedData += data.charCodeAt(i).toString(16).padStart(2, "0");
	    }
	}
	buf += encodedData;
    } else {
	buf += ',' + fmtHex(0) + CMD_DELIM_HEX;
    }
    return buf;
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

function parseResponse(input)
{
    var m = input.match(/>(\w),([0-9a-f]{8}),([0-9a-f]{8})(#|%)(.+)$/i);
    //console.log(m);
    if (m) {
	var r = m[0];
	var tail = m[5];
	var tailcrc = tail.substr(-8).toLowerCase();
	var resp = {
	    'command': m[1],
	    'arg':     parseInt(m[2], 16),
	    'length':  parseInt(m[3], 16),
	};
	// Zoom into CRC32 data
	var pdulen = 1 + 1 + 2 * (8 + 1) + 8 + (resp["length"] * 2);
	var data = r.substr(1, pdulen - 8); // Pdu ex crc
	crc32 = fmtHex(CRC32C.str(data)).toLowerCase();
	//console.log("Calc: " + crc32 + " - Rcv: " + tailcrc);
	if (crc32 == tailcrc) {
	    resp["crc"] = crc32;
	    resp["data"] = decodeData(m[4], tail.substr(0, tail.length - 8));
	} else {
	    console.log("CRC error: %s", r);
	    console.log("CRC error: %s", data);
	    console.log("CRC error: Have %s want %s", tailcrc, crc32);
	    throw "CRC error";
	}
	if (resp["length"] != resp["data"].length) {
	    throw "Length sanity error";
	}
	return resp;
    }
    return null;
}

async function portOpen(port, baud)
{
    await port.open({ baudRate: baud});
    const textDecoder = new TextDecoderStream();
    const readableStreamClosed = port.readable.pipeTo(textDecoder.writable);
    const reader = textDecoder.readable
	  .pipeThrough(new TransformStream(new BootstrapRequestTransformer()))
	  .getReader();

    return reader;
}

async function sendRequest(port, buf)
{
    var outArray = encodeString2Array(buf);
    var crc32 = fmtHex(CRC32C.buf(outArray));
    if (tracing)
	console.log("Req: " + buf + crc32);
    const encoder = new TextEncoder();
    const writer = port.writable.getWriter();
    await writer.write(encoder.encode(CMD_SOF));
    await writer.write(outArray);
    await writer.write(encoder.encode(crc32));
    writer.releaseLock();
}

async function completeRequest(port, req_str)
{
    // Signal busy
    document.getElementById("active").style.display = 'inline';
    // Send request
    await sendRequest(port, req_str);
    // Get response from port_reader stream
    var response = await port_reader.read();
    //console.log("Response: %o", response);
    // Signal received req
    document.getElementById("active").style.display = 'none';
    try {
	var rspStruct = parseResponse(response.value);
	//console.log("Response: %o", rspStruct);
	if (rspStruct["command"] == CMD_ACK) {
	    //console.log("Command acked: %d, crc = %s", rspStruct["arg"], rspStruct["crc"]);
	    return rspStruct;
	} else if (rspStruct["command"] == CMD_NACK) {
	    console.log("NACK: %o", rspStruct);
	    throw rspStruct["data"] ? rspStruct["data"] : "Unspecific NACK";
	} else {
	    console.log("Request failed to ack: %o", rspStruct);
	    throw "Request failed to ack";
	}
    } catch(e) {
	console.log("completeRequest: " + e);
	throw e;
    }
    return null;
}

async function downloadApp(port, appdata, binary)
{
    var completed = true;
    setStatus("Downloading " + appdata.length + " bytes " + (binary ? "binary" : "hex encoded") );
    try {
	const chunkSize = 256;
	let bytesSent = 0;

	await completeRequest(port, fmtReq(CMD_SEND, appdata.length));

	// Send data chunks
	while (bytesSent < appdata.length) {
	    let chunk = appdata.substr(bytesSent, chunkSize);
	    //console.log("Sending at offset: %d", bytesSent);
	    await completeRequest(port, fmtReq(CMD_DATA, bytesSent, chunk, binary));
	    bytesSent += chunk.length;
	    if (bytesSent % (5 * 1024))
		setStatus("Sent " + bytesSent + " bytes (" + (bytesSent * 100 / appdata.length).toFixed().toString(10) + "%)", true);
	    //console.log("Sent bytes: %d", chunk.length);
	}
    } catch (e) {
	addTrace("Download failed: " + e);
	completed = false;
    }
    setStatus("Download was " + (completed ? "completed" : "aborted"));
    return completed;
}

async function delayWait(timeout)
{
    return new Promise((resolve, reject) => setTimeout(resolve, timeout)).catch(alert);
}

async function delaySkipInput(port, timeout)
{
    await delayWait(timeout);
}

function addTrace(s)
{
    var trc = document.getElementById('log');
    console.log(s);
    trc.textContent += s + '\n'
    trc.scrollTop = trc.scrollHeight; // Scroll down
}

function setStatus(s, notrace)
{
    if (!notrace)
	addTrace(s);
    document.getElementById('status').innerHTML = s;
}

function setOpStatus(id, s)
{
    document.getElementById(id).innerHTML = s;
}

function setFeedbackStatus(id, s)
{
    console.log(s);
    setStatus(s);
    setOpStatus(id, s);
}

async function doWrite(port, operation, cmd, dev, status_id)
{
    let s = disableButtons("bl2u", true);
    try {
	setFeedbackStatus(status_id, "Starting " + operation);
	addTrace("This may take up to 5 minutes or even longer depending on data size and media.");
	let write = await completeRequest(port, fmtReq(cmd, dev));
	setFeedbackStatus(status_id, "Completed " + operation);
    } catch(e) {
	setFeedbackStatus(status_id, "Failed " + operation + ": " + e);
    } finally {
	restoreButtons(s);
    }
}

function disableButtons(stage, disable)
{
    let div = document.getElementById(stage);
    if (div) {
	let aBtn = div.querySelectorAll("button, input, select");
	let state = new Map();
	for (var btn of aBtn) {
	    state.set(btn.id, btn.disabled);
	    btn.disabled = disable;
	}
	return state;
    }
    return null;
}

function restoreButtons(state)
{
    for (let [key, value] of state) {
	let btn = document.getElementById(key);
	if (btn)
	    btn.disabled = value;
    }
}

function setStage(new_stage)
{
    var reset_state = (cur_stage != "adv_settings");
    var stages = ["connect", "bl1", "bl2u", "bl2u_ddr", "adv_settings", "booting"];
    for (const stage of stages) {
	var display = (new_stage == stage ? "block" : "none");
	document.getElementById(stage).style.display = display;
    };
    if (reset_state) {
	if (new_stage == "bl1") {
	    document.getElementById('bl1_sjtag_unlock').disabled = true;
	} else {
	    disableButtons(new_stage, false);
	}
    }
    cur_stage = new_stage;
}

async function doOtpRandom(port, stage, fb_id, sel_id)
{
	let s = disableButtons(stage, true);
	try {
	    setOpStatus(fb_id, "");
	    var fld = otp_fields[document.getElementById(sel_id).value];
	    var off = fld["offset"];
	    var len = fld["size"];
	    if (!confirm("Programming the '" + fld["name"] + "' is an irreversible action, " +
			 "which will affect the operation of your device!\n\n" +
                         "Can you confirm this?"))
		throw "OTP write aborted by user cancel";
	    var rspStruct = await completeRequest(port, fmtReq(CMD_OTPR, off, ntohl(len), false));
	    setStatus("OTP set random data completed");
	    setOpStatus(fb_id, "Wrote random data to the " + fld["name"]);
	} catch(e) {
	    setStatus(e);
	    setOpStatus(fb_id, e);
	} finally {
	    restoreButtons(s);
	}
}

async function doOtpSetData(port, stage, fb_id, sel_id, data_id)
{
	let s = disableButtons(stage, true);
	try {
	    setOpStatus(fb_id, "");
	    var fld = otp_fields[document.getElementById(sel_id).value];
	    var off = fld["offset"];
	    var len = fld["size"];
	    var buf = document.getElementById(data_id).value;
	    var data = hexString2Bin(buf);
	    if (data.length != len) {
		document.getElementById(data_id).focus();
		setOpStatus(fb_id, "Data should have " + len + " data bytes.");
	    } else {
		if (!confirm("Programming the '" + fld["name"] + "' is an irreversible action, " +
			     "which will affect the operation of your device!\n\n" +
                             "Can you confirm this?"))
		    throw "OTP write aborted by user cancel";
		var rspStruct = await completeRequest(port, fmtReq(CMD_OTPD, off, data, false));
		setStatus("OTP write completed");
		setOpStatus(fb_id, "Wrote data to the " + fld["name"]);
		addTrace("OTP set '" + fld["name"] + "' = '" + buf + "'");
	    }
	} catch(e) {
	    setStatus("OTP Set: " + e);
	    setOpStatus(fb_id, e);
	} finally {
	    restoreButtons(s);
	}
}

function otpSelPopulate(name, usemask)
{
    const sel = document.getElementById(name);
    if (sel) {
	for (var i = 0; i < otp_fields.length; i++) {
	    f = otp_fields[i];
	    use = f["use"] || 0;
	    if (usemask == undefined || (use & usemask))
		sel.options[sel.options.length] = new Option(f["name"], i);
	}
    }
}

const chunk = function(array, size) {
  if (!array.length) {
    return [];
  }
  const head = array.slice(0, size);
  const tail = array.slice(size);

  return [head, ...chunk(tail, size)];
};

function appendTd(tr, type, text)
{
    var td = document.createElement(type);
    td.appendChild(document.createTextNode(text));
    tr.appendChild(td);
}

function appendTds(tr, type, a)
{
    for (let v of a) {
	appendTd(tr, type, v);
    }
}

function createRegHelp(regname, reg)
{
    var container = document.createElement("div");
    var elm;

    container.classList.add("ddr_reghelp");

    elm = document.createElement("h3");
    elm.setAttribute("id", "ddr_help_current");
    elm.appendChild(document.createTextNode(regname));

    container.appendChild(elm);

    elm = document.createElement("p");
    elm.appendChild(document.createTextNode(reg.help));
    container.appendChild(elm);

    elm = document.createElement("table");
    const th = document.createElement("tr");
    appendTds(th, "th", ["Field", "Position", "Width", "Default", "Documentation"]);
    elm.appendChild(th);
    for (let [fldname, fld] of reg.fields) {
	const tr = document.createElement("tr");
	appendTds(tr, "td", [fldname.toLowerCase(), fld.pos, fld.width, fld.default, fld.help])

	elm.appendChild(tr);
    }
    container.appendChild(elm);

    return container;
}

function ddrUIsetup(name, prefix, plf)
{
    const topdiv = document.getElementById(name);
    var tabno = 0;

    for (let [grpname, regs] of plf["ddr_cfg"]) {
	tabno++;
	const tabname = prefix + "-tab-" + tabno;

	const t = document.createElement("div");
	t.classList.add("tab");

	const r = document.createElement("input");
	r.setAttribute("type", "radio");
	r.setAttribute("name", "css-tabs");
	r.setAttribute("id", tabname);
	r.classList.add("tab-switch");
	t.appendChild(r);

	const l = document.createElement("label");
	l.classList.add("tab-label");
	l.setAttribute("for", tabname);
	l.appendChild(document.createTextNode(grpname));
	t.appendChild(l);

	const c = document.createElement("div");
	c.classList.add("tab-content");

	const newTab = document.createElement("table");
	newTab.setAttribute("class", "reg_form");

	// Make x cols for each
	var col = 1;
	if (regs.length > 12)
	    col = 4;
	else if (regs.length > 5)
	    col = 3;

	regchunks = chunk(regs, col);
	for (let rowregs of regchunks) {
	    const tr = document.createElement("tr");

	    for (let regname of rowregs) {
		const rt = document.createElement("span");
		rt.appendChild(document.createTextNode(regname));
		const reg_desc = plf["ddr_regs"].get(regname);
		if (reg_desc) {
		    rt.addEventListener('click', async () => {
			const helpdiv = document.getElementById("bl2u_ddr_reghelp");
			const cur = document.getElementById("ddr_help_current");
			if (cur && cur.textContent == regname) {
			    helpdiv.innerHTML = "";
			    helpdiv.style.display = 'none';
			} else {
			    const helptext = createRegHelp(regname, reg_desc);
			    helpdiv.innerHTML = "";
			    helpdiv.appendChild(helptext);
			    helpdiv.style.display = 'inline';
			}
		    });
		}

		const tdt = document.createElement("td");
		tdt.setAttribute("class", "reg_text_col");
		tdt.appendChild(rt);
		tr.appendChild(tdt);

		const tdi = document.createElement("td");
		tdi.classList.add("reg_input_col");
		const ri = document.createElement("input");
		ri.setAttribute("id", regname);
		if (regname == "version") {
		    ri.classList.add("input_wide");
		    ri.setAttribute("maxlength", "127");
		} else {
		    ri.classList.add("input_normal");
		}
		tdi.appendChild(ri);
		tr.appendChild(tdi);
	    }
	    newTab.appendChild(tr);
	}

	c.appendChild(newTab);
	t.appendChild(c);
	topdiv.appendChild(t);
    }
}

window.addEventListener("load", (event) => {
    otpSelPopulate("bl1_otp_set_rnd_fld", otp_flag_rnd);
    otpSelPopulate("bl1_otp_set_data_fld", otp_flag_set);
    otpSelPopulate("bl2u_otp_set_rnd_fld", otp_flag_rnd);
    otpSelPopulate("bl2u_otp_set_data_fld", otp_flag_set);
    otpSelPopulate("bl2u_otp_read_fld");
});

function browserCheck()
{
    if (!navigator.userAgentData)
	return false;

    for (var b of navigator.userAgentData.brands) {
	console.log(b);
	if (b.brand.match(/chrome|chromium|crios/i)){
            return true;
	} else if (b.brand.match(/edg/i)){
            return true;
	}
    }

    return false;
}

function convertYaml(yaml, template)
{
    const cfg = new Map();

    for (let [grpname, regs] of template) {
	const grpMap = new Map();
	for (let regname of regs) {
	    let keys = yaml[grpname];
	    if (!keys)
		keys = yaml['config'];
	    if (keys[regname] != null) {
		grpMap.set(regname, keys[regname]);
	    } else {
		throw("Config not found: " + grpname + "." + regname);
	    }
	}
	cfg.set(grpname, grpMap);
    }

    return cfg;
}

function extractUint32(data, off)
{
    var arr = data.substr(off, 4);

    let val = 0;
    for (let i = arr.length - 1; i >= 0; i--) {
	val = (val << 8) + arr[i].charCodeAt(0);
    }
    return val >>> 0;
}

function convertFromBinary(data, template)
{
    const cfg = new Map();
    let off = 0;

    for (let [grpname, regs] of template) {
	const grpMap = new Map();
	for (let regname of regs) {
	    let value = null;
	    if (regname == "version") {
		// Special case
		value = data.substr(off, 128);
		let zero = value.indexOf(String.fromCharCode(0));
		if (zero)
		    value = value.substr(0, zero);
		off += 128;
	    } else {
		value = extractUint32(data, off);
		if (regname == "mem_size_mb") // Special case
		    value /= (1024 * 1024);
		off += 4;
	    }
	    grpMap.set(regname, value);
	}
	cfg.set(grpname, grpMap);
    }

    return cfg;
}

function logHexData(data)
{
    // Convert data to hex string for display
    var _data = data.split('').map(function (ch) {
	return ch.charCodeAt(0).toString(16).padStart(2, "0");
    }).join(":");
    console.log(_data);
}

function ntohl_val(arg)
{
    var ret = new Array(4);
    for (var i = 0, j = 0; i <= 24; i += 8, j++)
	ret[j] = (0xFF & (arg >>> i)) >>> 0;
    return ret;
}

function convertToBinary(cfg)
{
    let buf = new Array();

    for (let [grp, keys] of cfg) {
	for (let [reg, value] of keys) {
	    if (reg == "version")
		value = value.padEnd(128, String.fromCharCode(0)).split('').map(
		    function (ch) {
			return ch.charCodeAt(0);
		    }
		);
	    else {
		if (reg == "mem_size_mb")
		    value *= (1024 * 1024); // Special case
		value = ntohl_val(value);
	    }
	    buf = buf.concat(value);
	}
    }
    return buf.map(function (ch) {
        return String.fromCharCode(ch);
    }).join("");
}

function populateCfg(template)
{
    for (let [grp, keys] of template) {
	for (let [reg, value] of keys) {
	    let inp = document.getElementById(reg);
	    if (inp) {
		if (grp === "info")
		    inp.value = value;
		else
		    inp.value = "0x" + (value.toString(16).toUpperCase().padStart(8,"0"));
	    } else {
		throw("Unable to find: " + grp + "." + reg);
	    }
	}
    }
}

function getDDRFromForm(template)
{
    const cfg = new Map();

    for (let [grpname, keys] of template) {
	const grpMap = new Map();
	for (let regname of keys) {
	    let inp = document.getElementById(regname);
	    let value = null;
	    if (inp) {
		if (regname === "version")
		    value = inp.value;
		else
		    value = parseInt(inp.value);
		grpMap.set(regname, value);
	    } else {
		throw("Unable to find: " + grpname + "." + regname);
	    }
	}
	cfg.set(grpname, grpMap);
    }
    return cfg;
}

// Convert Map objects to pure JS objects
function map2obj(m)
{
    var cfg = {};
    for (let [grpname, regs] of m) {
	var grp = {};
	for (let [regname, regval] of regs) {
	    grp[regname] = regval;
	}
	cfg[grpname] = grp;
    }
    return cfg;
}

// Save text to file
async function saveFile(fileData)
{
    const opts = {
	types: [
	    {
		description: "YAML output file",
		accept: { "text/yaml": [".yaml"] },
	    },
	],
    };

    // create a new handle
    const newHandle = await window.showSaveFilePicker(opts);

    // create a FileSystemWritableFileStream to write to
    const writableStream = await newHandle.createWritable();

    // write our file
    await writableStream.write(fileData);

    // close the file and write the contents to disk.
    await writableStream.close();
}

function startSerial()
{
    var port;
    var image;
    var sjtag_challenge;
    var settings_prev_stage;
    var plf;

    if (!browserCheck()) {
	document.getElementById('browser_check').style.display = 'block';
	document.getElementById('connect').style.display = 'none';
	console.log("Browser check failed, bailing out. Use Chrome or Edge.");
	return;
    }

    document.getElementById("file_select").addEventListener("change", function () {
	if (this.files && this.files[0]) {
	    var myFile = this.files[0];
	    var reader = new FileReader();

	    reader.addEventListener('load', function (e) {
		console.log("Read Image: %d bytes", e.total);
		image = e.target.result;
	    });

	    reader.readAsBinaryString(myFile);
	}
    });

    document.getElementById('bl2u_upload').addEventListener('click', async () => {
	if (image.length) {
	    let s = disableButtons("bl2u", true);
	    try {
		await downloadApp(port, image, document.getElementById("binary").checked);
		// Do explicit uncompress
		setStatus("Decompressing");
		var rspStruct = await completeRequest(port, fmtReq(CMD_UNZIP, 0));
		var datalen = rspStruct["arg"].toString(16).padStart(8, "0");
		var status = rspStruct["data"];
		setStatus("Data received: " + status + ", length " + rspStruct["arg"].toString(10));
	    } catch(e) {
		setStatus("Failed upload: " + e);
	    } finally {
		restoreButtons(s);
	    }
	}
    });

    document.getElementById('bl2u_bind').addEventListener('click', async () => {
	let s = disableButtons("bl2u", true);
	try {
	    if (!confirm("In order to perform 'Bind', the FIP image\n" +
			 "used must have been encrypted using the 'SSK' key.\n" +
			 "Can you confirm this?"))
		throw "Bind aborted by user cancel";
	    let bind = await completeRequest(port, fmtReq(CMD_BL2U_BIND, 0));
	    s.set('bl2u_bind', true); // Bind is a 'once' operation
	    setStatus("Bind completed sucessfully");
	} catch(e) {
	    setFeedbackStatus('bl2u_bind_fip_feedback', e);
	} finally {
	    restoreButtons(s);
	}
    });

    document.getElementById('bl2u_write_fip').addEventListener('click', async () => {
	let dev = document.getElementById('bl2u_write_fip_device');
	let op = "Write FIP to " + dev.selectedOptions[0].text;
	doWrite(port, op, CMD_BL2U_WRITE, dev.value, 'bl2u_write_fip_feedback');
    });

    document.getElementById('bl2u_write_image').addEventListener('click', async () => {
	let dev = document.getElementById('bl2u_write_image_device');
	let op = "Write Image to " + dev.selectedOptions[0].text;
	if (confirm("Programming flash image will not check whether " +
		    "the image contain valid data for the chosen device type.\n\n" +
                    "Can you confirm you want to perform this operation?"))
	    doWrite(port, op, CMD_BL2U_IMAGE, dev.value, 'bl2u_write_image_feedback');
    });

    document.getElementById('bl2u_otp_read').addEventListener('click', async () => {
	let s = disableButtons("bl2u", true);
	try {
	    setOpStatus("bl2u_otp_read_feedback", "");
	    var fld = otp_fields[document.getElementById('bl2u_otp_read_fld').value];
	    var off = fld["offset"];
	    var len = fld["size"];
	    var cmd = CMD_BL2U_OTP_READ;
	    var rspStruct = await completeRequest(port, fmtReq(cmd, off, ntohl(len), false));
	    setStatus("OTP read completed", true);
	    // Convert data to hex string for display
	    var data = rspStruct["data"].split('').map(function (ch) {
		return ch.charCodeAt(0).toString(16).padStart(2, "0");
	    }).join(":");
	    if (len < 32) {
		setOpStatus("bl2u_otp_read_feedback", data);
	    } else {
		setOpStatus("bl2u_otp_read_feedback", fld["name"] + " data is in trace");
	    }
	    addTrace("OTP read " + fld["name"] + " = " + data);
	} catch(e) {
	    setStatus("OTP Read: " + e);
	    setOpStatus("bl2u_otp_read_feedback", e);
	} finally {
	    restoreButtons(s);
	}
    });

    document.getElementById('bl2u_otp_set_rnd').addEventListener('click', async () => {
	await doOtpRandom(port, 'bl2u', 'bl2u_otp_set_rnd_feedback', 'bl2u_otp_set_rnd_fld');
    });

    document.getElementById('bl2u_otp_set_data').addEventListener('click', async () => {
	await doOtpSetData(port, 'bl2u', 'bl2u_otp_set_data_feedback', 'bl2u_otp_set_data_fld', 'bl2u_otp_set_data_buf');
    });

    document.getElementById('bl2u_ddr_mode_enter').addEventListener('click', async () => {
	setStage("bl2u_ddr");
    });

    document.getElementById('bl2u_ddr_load').addEventListener('click', async () => {
	let s = disableButtons("bl2u_ddr", true);
	try {
	    setStatus("DDR load from device start");
	    var rspStruct = await completeRequest(port, fmtReq(CMD_BL2U_DDR_CFG_GET, 0));
	    addTrace("DDR configuration data: " + rspStruct["data"].length + " bytes");
	    logHexData(rspStruct["data"]);
	    let cfg = convertFromBinary(rspStruct["data"], plf["ddr_cfg"]);
	    setStatus("DDR config read: " + cfg.get("info").get("version"));
	    populateCfg(cfg);
	} catch(e) {
	    setStatus("DDR load from device error: " + e);
	} finally {
	    restoreButtons(s);
	}
    });

    document.getElementById('bl2u_ddr_init').addEventListener('click', async () => {
	let s = disableButtons("bl2u_ddr", true);
	try {
	    setStatus("DDR initialize starting");
	    let cfg = getDDRFromForm(plf["ddr_cfg"]);
	    let data = convertToBinary(cfg);
	    logHexData(data);
	    var rspStruct = await completeRequest(port, fmtReq(CMD_BL2U_DDR_CFG_SET, 0, data));
	    setStatus("DDR was initialized: " + cfg.get("info").get("version"));
	} catch(e) {
	    setStatus("DDR initialize error: " + e);
	} finally {
	    restoreButtons(s);
	}
    });

    document.getElementById('bl2u_ddr_test').addEventListener('click', async () => {
	let s = disableButtons("bl2u_ddr", true);
	try {
	    setStatus("DDR test starting");
	    var msec_start = new Date().getTime();
	    var arg = document.getElementById("enable_cache").checked ? 1 : 0;
	    var rspStruct = await completeRequest(port, fmtReq(CMD_BL2U_DDR_TEST, arg));
	    var msec_end = new Date().getTime();
	    setStatus("DDR was tested: " + rspStruct["data"]);
	    addTrace("DDR test duration: " + (msec_end - msec_start) + " msec");
	} catch(e) {
	    setStatus("DDR test error: " + e);
	} finally {
	    restoreButtons(s);
	}
    });

    document.getElementById('bl2u_ddr_mode_exit').addEventListener('click', async () => {
	setStage("bl2u");
    });

    document.getElementById('bl2u_reset').addEventListener('click', async () => {
	let s = disableButtons("bl2u", true);
	try {
	    setStatus("Rebooting from BL2U back to BL1");
	    let cont = await completeRequest(port, fmtReq(CMD_BL2U_RESET, 0));
	    setStatus("Booting into BL1");
	} catch(e) {
	    setStatus("BL1 reset failed");
	    addTrace("Failure: " + e);
	    addTrace("You might need to reload page and reconnect");
	    try {
		setStatus("BL1: Trying to syncronize");
		await delaySkipInput(port, 2000);
		var rspStruct = await completeRequest(port, fmtReq(CMD_VERS, 0));
		setStatus("BL1: Syncronization succeeded");
	    } catch (d) {
		setStatus("BL1 reset failed, please reconnect");
	    }
	} finally {
	    await delaySkipInput(port, 2000);
	    setStage("bl1");
	    restoreButtons(s);
	}
    });

    document.getElementById('bl1_download').addEventListener('click', async () => {
	let s = disableButtons("bl1", true);
	try {
	    setStatus("Downloading BL2U applet");
	    const app = atob(plf["bl2u"].join(""));
	    if (app.length == 0)
		throw "Empty application image - no BL2U support for " + plf["name"];
	    await downloadApp(port, app, plf["bl1_binary"]);
	    await delaySkipInput(port, 2000);
	    let auth = await completeRequest(port, fmtReq(CMD_AUTH, 0));
	    setStatus("BL2U booting");
	    // Allow BL2U to boot
	    await delaySkipInput(port, 2000);
	    let fwu_vers = await completeRequest(port, fmtReq(CMD_VERS, 0));
	    setStatus("BL2U operational: " + fwu_vers["data"]);
	    setStage("bl2u");
	} catch(e) {
	    setStatus(e);
	} finally {
	    restoreButtons(s);
	}
    });

    document.getElementById('bl1_sjtag_challenge').addEventListener('click', async () => {
	let s = disableButtons("bl1", true);
	try {
	    var rspStruct = await completeRequest(port, fmtReq(CMD_SJTAG_RD, 0));
	    sjtag_challenge = rspStruct["data"];
	    var data = rspStruct["data"].split('').map(function (ch) {
		return ch.charCodeAt(0).toString(16).padStart(2, "0");
	    }).join(":");
	    setStatus("SJTAG Challenge read");
	    setOpStatus("bl1_sjtag_challenge_feedback", "Challenge received");
	    addTrace("SJTAG Challenge: " + data);
	    s.set('bl1_sjtag_unlock', false);
	} catch(e) {
	    setStatus("SJTAG Challenge: " + e);
	    setOpStatus("bl1_sjtag_challenge_feedback", "Failure: SJTAG is most likely not enabled in OTP");
	} finally {
	    restoreButtons(s);
	}
    });

    document.getElementById('bl1_sjtag_unlock').addEventListener('click', async () => {
	let s = disableButtons("bl1", true);
	try {
	    var key_inp = document.getElementById('bl1_sjtag_unlock_key');
	    var data;
	    try { data = hexString2Bin(key_inp.value); } catch (e) { data = ""; }
	    if (data.length != 32) {
		setOpStatus("bl1_sjtag_unlock_feedback", "Key invalid, please enter 32 bytes");
		key_inp.focus();
	    } else {
		// Now derive the key
		data = sha256(sjtag_challenge + data);
		// Go ahead and unlock
		var rspStruct = await completeRequest(port, fmtReq(CMD_SJTAG_WR, 0, data, false));
		setStatus("SJTAG Unlocked");
		setOpStatus("bl1_sjtag_unlock_feedback", "SJTAG Unlocked");
		setOpStatus("bl1_sjtag_challenge_feedback", "");
		s.set('bl1_sjtag_unlock', true);
		s.set('bl1_sjtag_challenge', true);
	    }
	} catch(e) {
	    setStatus("SJTAG Unlock: " + e);
	    setOpStatus("bl1_sjtag_unlock_feedback", "Failure: SJTAG did not unlock, key probably wrong");
	} finally {
	    restoreButtons(s);
	}
    });

    document.getElementById('bl1_otp_set_rnd').addEventListener('click', async () => {
	await doOtpRandom(port, 'bl1', 'bl1_otp_set_rnd_feedback', 'bl1_otp_set_rnd_fld');
    });

    document.getElementById('bl1_otp_set_data').addEventListener('click', async () => {
	await doOtpSetData(port, 'bl1', 'bl1_otp_set_data_feedback', 'bl1_otp_set_data_fld', 'bl1_otp_set_data_buf');
    });

    document.getElementById('port_select').addEventListener('click', async () => {
	port = await navigator.serial.requestPort();
	var baud = document.getElementById('baudrate').selectedOptions[0].value;
	port_reader = await portOpen(port, baud);
	setStatus("Connected");
	// Avoid reconnect
	document.getElementById('port_select').disabled = true;
	setStage("none");
	// Flush port for any pending boot massages
	await delaySkipInput(port, 200);

	try {
	    setStatus("Identify platform...");
	    var rspStruct = await completeRequest(port, fmtReq(CMD_VERS, 0));
	    var chip = rspStruct["arg"].toString(16).padStart(8, "0");
	    var version = rspStruct["data"];
	    addTrace("Version: " + version);
	    plf = getPlatform(chip);
	    if (plf) {
		setStatus("Identified device: " + plf["name"]);
		if (version.match(/^BL2:/)) {
		    addTrace("BL2U context detected");
		    setStage("bl2u");
		} else {
		    addTrace("Please select a BL1 command - or upload BL2U for firmware update functions");
		    setStage("bl1");
		}
	    } else {
		addTrace("Unidentified device");
		addTrace("Device: " + chip + ", " + version + "\n" +
			 "- which is not identified as a known device.\n" +
			 "Please reconnect.\n");
	    }
	    ddrUIsetup("bl2u_ddr_pane", "ddr", plf);
	} catch (e) {
	    console.log("Connect failed: " + e);
	}
    });

    document.getElementById('ddr_config_input').addEventListener('change', (e) => {
        if(!e.target.files[0]) {
            return;
        }

        var reader = new FileReader();
        // Set OnLoad callback
        reader.onload = function(e) {
	    // First, load raw file
            let loadedObj = jsyaml.load(e.target.result);

	    // Iterate config template, check yaml, populate form
	    try {
		let cfg = convertYaml(loadedObj, plf["ddr_cfg"]);
		// To form
		populateCfg(cfg);
	    } catch(e) {
		setStatus("Invalid DDR configuration");
		addTrace(e);
	    }

        };

        // Read file into memory as UTF-8
        reader.readAsText(e.target.files[0], "utf8");
    });

    document.getElementById('ddr_config_write').addEventListener('click', async () => {
	let cfg = getDDRFromForm(plf["ddr_cfg"]);
	if (cfg) {
	    try {
		saveFile(jsyaml.dump(map2obj(cfg)));
	    } catch(e) {
	    }
	} else {
	    alert("No DDR configuration loaded");
	}
    });

    document.getElementById('settings').addEventListener('click', async (event) => {
	if (settings_prev_stage) {
	    setStage(settings_prev_stage);
	    settings_prev_stage = undefined;
	} else {
	    settings_prev_stage = cur_stage;
	    setStage("adv_settings");
	}
    });

    document.getElementById('settings_back').addEventListener('click', async (event) => {
	if (settings_prev_stage) {
	    document.getElementById('settings').click();
	} else {
	    setStage("connect");
	}
    });

    document.getElementById('enable_trace').addEventListener('change', async (event) => {
	var inp = event.srcElement;
	tracing = inp.checked;
	console.log("trace setting: %o", tracing);
    });
}
