import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const outputDirectory = dirname(fileURLToPath(import.meta.url));
mkdirSync(outputDirectory, { recursive: true });

const colors = {
    background: "#f7faf9",
    panel: "#ffffff",
    ink: "#172a3a",
    muted: "#53656f",
    border: "#9dafb2",
    integer: "#277f75",
    fraction: "#e9b949",
    signed: "#d7664f",
    byte: "#457b9d",
    accent: "#7a5c99",
    padding: "#dce5e3"
};

const escapeXml = value => String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");

function text(x, y, value, options = {}) {
    const { size = 16, weight = 400, fill = colors.ink, anchor = "start", family = "sans" } = options;
    const font = family === "mono" ? "'Cascadia Code', 'IBM Plex Mono', monospace" : "'IBM Plex Sans', 'Segoe UI', sans-serif";
    return `<text x="${x}" y="${y}" font-family="${font}" font-size="${size}" font-weight="${weight}" fill="${fill}" text-anchor="${anchor}">${escapeXml(value)}</text>`;
}

function rect(x, y, width, height, fill, radius = 4, stroke = colors.border) {
    return `<rect x="${x}" y="${y}" width="${width}" height="${height}" rx="${radius}" fill="${fill}" stroke="${stroke}"/>`;
}

function document({ title, description, height, body }) {
    return `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="1120" height="${height}" viewBox="0 0 1120 ${height}" role="img" aria-labelledby="title description">
  <title id="title">${escapeXml(title)}</title>
  <desc id="description">${escapeXml(description)}</desc>
  <rect width="1120" height="${height}" fill="${colors.background}"/>
  ${body.join("\n  ")}
</svg>
`;
}

function writeSvg(filename, spec) {
    writeFileSync(join(outputDirectory, filename), document(spec), "utf8");
}

function renderBitLayout({ filename, title, subtitle, totalBits, fields, signed = false, footer }) {
    const body = [];
    const left = 48;
    const width = 1024;
    const bytes = totalBits / 8;

    body.push(text(left, 42, title, { size: 25, weight: 700 }));
    body.push(text(left, 70, subtitle, { size: 15, fill: colors.muted }));
    body.push(text(left, 100, "Logical value bits (most significant to least significant)", { size: 14, weight: 600 }));

    let bitOffset = 0;
    for (const field of fields) {
        const fieldWidth = width * field.bits / totalBits;
        const x = left + bitOffset * width / totalBits;
        body.push(rect(x, 112, fieldWidth, 54, field.color));
        body.push(text(x + fieldWidth / 2, 145, field.label, {
            size: fieldWidth < 70 ? 13 : 16,
            weight: 700,
            fill: field.color === colors.fraction ? colors.ink : "#ffffff",
            anchor: "middle",
            family: "mono"
        }));
        bitOffset += field.bits;
    }

    const legend = fields.map(field => `${field.label}: ${field.detail}`).join("   |   ");
    body.push(text(left, 190, legend, { size: 13, fill: colors.muted, family: "mono" }));
    body.push(text(left, 220, "Wire bytes (in increasing address/transmission order)", { size: 14, weight: 600 }));

    const gap = 8;
    const byteWidth = (width - gap * (bytes - 1)) / bytes;
    for (let index = 0; index < bytes; index++) {
        const x = left + index * (byteWidth + gap);
        body.push(rect(x, 232, byteWidth, 52, index === 0 ? colors.byte : colors.panel));
        body.push(text(x + byteWidth / 2, 255, `byte ${index}`, {
            size: 14,
            weight: 700,
            fill: index === 0 ? "#ffffff" : colors.ink,
            anchor: "middle",
            family: "mono"
        }));
        body.push(text(x + byteWidth / 2, 275, `bits ${index * 8 + 7}..${index * 8}`, {
            size: 11,
            fill: index === 0 ? "#ffffff" : colors.muted,
            anchor: "middle",
            family: "mono"
        }));
    }

    body.push(text(left, 310, `Little-endian: byte 0 carries the least-significant 8 bits.${signed ? " Signed values use two's complement." : ""}`, { size: 14, fill: colors.muted }));
    if (footer) body.push(text(left, 336, footer, { size: 14, weight: 600 }));

    writeSvg(filename, {
        title,
        description: `${title}. ${subtitle}. Little-endian ${totalBits}-bit storage layout.`,
        height: footer ? 360 : 334,
        body
    });
}

const scalarLayouts = [
    ["uint8.svg", "uint8_t / u8", "Unsigned 8-bit integer; 1 byte", 8, [{ label: "value", bits: 8, detail: "bits 7..0", color: colors.integer }], false, "Range: 0 to 255"],
    ["uint16.svg", "uint16_t / u16", "Unsigned 16-bit integer; 2 bytes", 16, [{ label: "value", bits: 16, detail: "bits 15..0", color: colors.integer }], false, "Range: 0 to 65,535"],
    ["int16.svg", "int16_t / i16", "Signed 16-bit two's-complement integer; 2 bytes", 16, [{ label: "S", bits: 1, detail: "sign weight -2^15", color: colors.signed }, { label: "value", bits: 15, detail: "remaining weights", color: colors.integer }], true, "Range: -32,768 to 32,767"],
    ["uint32.svg", "uint32_t / u32", "Unsigned 32-bit integer; 4 bytes", 32, [{ label: "value", bits: 32, detail: "bits 31..0", color: colors.integer }], false, "Range: 0 to 4,294,967,295"],
    ["int32.svg", "int32_t / i32", "Signed 32-bit two's-complement integer; 4 bytes", 32, [{ label: "S", bits: 1, detail: "sign weight -2^31", color: colors.signed }, { label: "value", bits: 31, detail: "remaining weights", color: colors.integer }], true, "Range: -2,147,483,648 to 2,147,483,647"],
    ["uint64.svg", "uint64_t device identifier", "Unsigned 64-bit identifier; 8 bytes", 64, [{ label: "identifier", bits: 64, detail: "bits 63..0", color: colors.accent }], false, "Used by Jacdac frames; it is an identifier, not a quantity"]
];

for (const [filename, title, subtitle, totalBits, fields, signed, footer] of scalarLayouts) {
    renderBitLayout({ filename, title, subtitle, totalBits, fields, signed, footer });
}

const fixedLayouts = [
    ["u0-8.svg", "u0.8", "Unsigned binary fixed point in 8 bits", 8, [{ label: "fraction", bits: 8, detail: "weights 2^-1 through 2^-8", color: colors.fraction }], false, "Decode: raw / 256   |   LSB: 1/256   |   Range: 0 to 255/256"],
    ["u0-16.svg", "u0.16", "Unsigned binary fixed point in 16 bits", 16, [{ label: "fraction", bits: 16, detail: "weights 2^-1 through 2^-16", color: colors.fraction }], false, "Decode: raw / 65,536   |   LSB: 1/65,536   |   Range: 0 to 65,535/65,536"],
    ["i1-15.svg", "i1.15", "Signed binary fixed point in 16 bits", 16, [{ label: "S", bits: 1, detail: "sign weight -1", color: colors.signed }, { label: "fraction", bits: 15, detail: "weights 2^-1 through 2^-15", color: colors.fraction }], true, "Decode: raw / 32,768   |   LSB: 2^-15   |   Range: -1 to 1 - 2^-15"],
    ["i12-20.svg", "i12.20", "Signed binary fixed point in 32 bits", 32, [{ label: "signed whole", bits: 12, detail: "sign plus 11 integer bits", color: colors.signed }, { label: "fraction", bits: 20, detail: "weights 2^-1 through 2^-20", color: colors.fraction }], true, "Decode: raw / 1,048,576   |   LSB: 2^-20   |   Range: -2,048 to 2,048 - 2^-20"],
    ["i22-10.svg", "i22.10", "Signed binary fixed point in 32 bits", 32, [{ label: "signed whole", bits: 22, detail: "sign plus 21 integer bits", color: colors.signed }, { label: "fraction", bits: 10, detail: "weights 2^-1 through 2^-10", color: colors.fraction }], true, "Decode: raw / 1,024   |   LSB: 2^-10   |   Range: -2,097,152 to 2,097,152 - 2^-10"],
    ["u22-10.svg", "u22.10", "Unsigned binary fixed point in 32 bits", 32, [{ label: "whole", bits: 22, detail: "unsigned integer bits", color: colors.integer }, { label: "fraction", bits: 10, detail: "weights 2^-1 through 2^-10", color: colors.fraction }], false, "Decode: raw / 1,024   |   LSB: 2^-10   |   Range: 0 to 4,194,304 - 2^-10"],
    ["i16-16.svg", "i16.16 / signed Q16.16", "Signed binary fixed point in 32 bits", 32, [{ label: "signed whole", bits: 16, detail: "sign plus 15 integer bits", color: colors.signed }, { label: "fraction", bits: 16, detail: "weights 2^-1 through 2^-16", color: colors.fraction }], true, "Decode: raw / 65,536   |   LSB: 2^-16   |   Range: -32,768 to 32,768 - 2^-16"],
    ["u16-16.svg", "u16.16", "Unsigned binary fixed point in 32 bits", 32, [{ label: "whole", bits: 16, detail: "unsigned integer bits", color: colors.integer }, { label: "fraction", bits: 16, detail: "weights 2^-1 through 2^-16", color: colors.fraction }], false, "Decode: raw / 65,536   |   LSB: 2^-16   |   Range: 0 to 65,536 - 2^-16"]
];

for (const [filename, title, subtitle, totalBits, fields, signed, footer] of fixedLayouts) {
    renderBitLayout({ filename, title, subtitle, totalBits, fields, signed, footer });
}

function renderByteCells({ filename, title, subtitle, cells, footer, description = subtitle }) {
    const body = [
        text(48, 42, title, { size: 25, weight: 700 }),
        text(48, 70, subtitle, { size: 15, fill: colors.muted })
    ];
    const left = 48;
    const top = 104;
    const gap = 8;
    const width = 1024;
    const cellWidth = (width - gap * (cells.length - 1)) / cells.length;
    cells.forEach((cell, index) => {
        const x = left + index * (cellWidth + gap);
        body.push(rect(x, top, cellWidth, 76, cell.color || colors.panel));
        body.push(text(x + cellWidth / 2, top + 31, cell.label, { size: 16, weight: 700, anchor: "middle", family: "mono", fill: cell.dark ? "#ffffff" : colors.ink }));
        body.push(text(x + cellWidth / 2, top + 56, cell.detail, { size: 12, anchor: "middle", family: "mono", fill: cell.dark ? "#ffffff" : colors.muted }));
    });
    body.push(text(48, 214, footer, { size: 14, weight: 600 }));
    writeSvg(filename, { title, description, height: 242, body });
}

renderByteCells({
    filename: "bool-u8.svg",
    title: "Boolean on the Jacdac wire",
    subtitle: "C++ bool arguments are encoded as one u8 byte by the typed client",
    cells: [{ label: "0x00 or 0x01", detail: "byte 0", color: colors.integer, dark: true }],
    footer: "0 means false; 1 means true. Do not memcpy a platform-native bool into a generic payload."
});

renderByteCells({
    filename: "enum-u8.svg",
    title: "Enum on the Jacdac wire",
    subtitle: "The public service enums in this library have uint8_t backing storage",
    cells: [{ label: "enum code", detail: "byte 0", color: colors.accent, dark: true }],
    footer: "The byte is interpreted using the enum declared by that register's service specification."
});

renderByteCells({
    filename: "bytes.svg",
    title: "Variable byte buffer / bytes",
    subtitle: "Opaque payload bytes preserve order; the surrounding packet supplies the length",
    cells: [
        { label: "byte 0", detail: "first" },
        { label: "byte 1", detail: "second" },
        { label: "...", detail: "variable", color: colors.padding },
        { label: "byte N-1", detail: "last" }
    ],
    footer: "No pointer value is transmitted. The bytes referenced by the pointer are copied into the packet."
});

renderByteCells({
    filename: "rgb.svg",
    title: "Packed RGB color",
    subtitle: "LED values use three adjacent u8 color operands in this order",
    cells: [
        { label: "red", detail: "byte 0", color: "#d7664f", dark: true },
        { label: "green", detail: "byte 1", color: "#4c956c", dark: true },
        { label: "blue", detail: "byte 2", color: "#457b9d", dark: true }
    ],
    footer: "Each channel is 0..255. This is byte packing, not a native 24- or 32-bit integer."
});

renderByteCells({
    filename: "acceleration-xyz.svg",
    title: "Accelerometer reading payload",
    subtitle: "Three consecutive little-endian i12.20 values; 12 bytes total",
    cells: [
        { label: "X: i12.20", detail: "bytes 0..3", color: colors.signed, dark: true },
        { label: "Y: i12.20", detail: "bytes 4..7", color: colors.integer, dark: true },
        { label: "Z: i12.20", detail: "bytes 8..11", color: colors.accent, dark: true }
    ],
    footer: "Decode each signed int32_t independently as raw / 1,048,576 g."
});

renderByteCells({
    filename: "vibration-sequence.svg",
    title: "Vibration sequence payload",
    subtitle: "A repeated two-byte tuple; no count byte is stored inside the payload",
    cells: [
        { label: "duration", detail: "u8 x 8 ms", color: colors.byte, dark: true },
        { label: "intensity", detail: "u0.8", color: colors.fraction },
        { label: "duration", detail: "u8 x 8 ms", color: colors.byte, dark: true },
        { label: "intensity", detail: "u0.8", color: colors.fraction },
        { label: "...", detail: "repeat", color: colors.padding }
    ],
    footer: "Payload length / 2 gives the number of steps. An empty sequence requests stop."
});

{
    const body = [
        text(48, 42, "float API input to i16.16 wire value", { size: 25, weight: 700 }),
        text(48, 70, "ServoClient::setAngle() accepts float degrees, then converts before queueing", { size: 15, fill: colors.muted }),
        rect(48, 106, 252, 76, colors.panel),
        text(174, 137, "float degrees", { size: 17, weight: 700, anchor: "middle", family: "mono" }),
        text(174, 162, "API-only value", { size: 13, fill: colors.muted, anchor: "middle" }),
        text(335, 151, "x 65,536", { size: 17, weight: 700, anchor: "middle", fill: colors.accent, family: "mono" }),
        text(450, 151, "->", { size: 24, weight: 700, anchor: "middle", fill: colors.muted, family: "mono" }),
        rect(500, 106, 572, 76, colors.signed),
        text(786, 137, "int32_t i16.16", { size: 17, weight: 700, anchor: "middle", family: "mono", fill: "#ffffff" }),
        text(786, 162, "4 little-endian wire bytes", { size: 13, anchor: "middle", fill: "#ffffff" }),
        text(48, 218, "The nRF target's float object is not transmitted. floatToQ16() truncates toward zero after scaling.", { size: 14, weight: 600 })
    ];
    writeSvg("float-to-i16-16.svg", {
        title: "Float API input converted to signed i16.16 storage",
        description: "A floating point angle is multiplied by 65536 and stored as a four-byte little-endian signed i16.16 integer.",
        height: 246,
        body
    });
}

{
    const body = [
        text(48, 42, "Jacdac frame and packet storage", { size: 25, weight: 700 }),
        text(48, 70, "Frame fields are shared by one or more 4-byte-aligned service packets", { size: 15, fill: colors.muted }),
        text(48, 102, "Frame: 12-byte header followed by frame_size bytes", { size: 14, weight: 600 }),
        rect(48, 116, 150, 64, colors.signed),
        text(123, 143, "CRC16", { size: 16, weight: 700, anchor: "middle", family: "mono", fill: "#ffffff" }),
        text(123, 164, "bytes 0..1", { size: 11, anchor: "middle", fill: "#ffffff" }),
        rect(206, 116, 100, 64, colors.byte),
        text(256, 143, "size", { size: 15, weight: 700, anchor: "middle", family: "mono", fill: "#ffffff" }),
        text(256, 164, "byte 2", { size: 11, anchor: "middle", fill: "#ffffff" }),
        rect(314, 116, 100, 64, colors.accent),
        text(364, 143, "flags", { size: 15, weight: 700, anchor: "middle", family: "mono", fill: "#ffffff" }),
        text(364, 164, "byte 3", { size: 11, anchor: "middle", fill: "#ffffff" }),
        rect(422, 116, 330, 64, colors.integer),
        text(587, 143, "device identifier", { size: 16, weight: 700, anchor: "middle", family: "mono", fill: "#ffffff" }),
        text(587, 164, "bytes 4..11 (u64 LE)", { size: 11, anchor: "middle", fill: "#ffffff" }),
        rect(760, 116, 312, 64, colors.panel),
        text(916, 143, "frame data", { size: 16, weight: 700, anchor: "middle", family: "mono" }),
        text(916, 164, "bytes 12..", { size: 11, anchor: "middle", fill: colors.muted }),
        text(48, 222, "Each packet inside frame data", { size: 14, weight: 600 }),
        rect(48, 236, 166, 64, colors.byte),
        text(131, 263, "payload size", { size: 15, weight: 700, anchor: "middle", family: "mono", fill: "#ffffff" }),
        text(131, 284, "+0: u8", { size: 11, anchor: "middle", fill: "#ffffff" }),
        rect(222, 236, 166, 64, colors.accent),
        text(305, 263, "service index", { size: 15, weight: 700, anchor: "middle", family: "mono", fill: "#ffffff" }),
        text(305, 284, "+1: low 6 bits", { size: 11, anchor: "middle", fill: "#ffffff" }),
        rect(396, 236, 238, 64, colors.integer),
        text(515, 263, "service command", { size: 15, weight: 700, anchor: "middle", family: "mono", fill: "#ffffff" }),
        text(515, 284, "+2..3: u16 LE", { size: 11, anchor: "middle", fill: "#ffffff" }),
        rect(642, 236, 276, 64, colors.panel),
        text(780, 263, "payload", { size: 15, weight: 700, anchor: "middle", family: "mono" }),
        text(780, 284, "+4..: 0..236 bytes", { size: 11, anchor: "middle", fill: colors.muted }),
        rect(926, 236, 146, 64, colors.padding),
        text(999, 263, "padding", { size: 15, weight: 700, anchor: "middle", family: "mono" }),
        text(999, 284, "0..3 bytes", { size: 11, anchor: "middle", fill: colors.muted }),
        text(48, 338, "All multibyte fields are little-endian. Padding is not part of service_size; it aligns the next packet to 4 bytes.", { size: 14, weight: 600 })
    ];
    writeSvg("frame-packet.svg", {
        title: "Jacdac frame and packet wire storage",
        description: "A 12-byte frame header contains CRC, size, flags, and device identifier. Each aligned packet contains payload size, service index, service command, payload, and padding.",
        height: 370,
        body
    });
}

console.log(`Generated ${scalarLayouts.length + fixedLayouts.length + 8} SVG layouts in ${outputDirectory}`);