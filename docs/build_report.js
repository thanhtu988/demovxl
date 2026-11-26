// Chuyen report_C.md -> BaoCao_phanC_nhap.docx, dinh dang giong Template_Report_BTL_CO3009.docx
// (Times New Roman 13 pt, bang header D9E2F3, code Consolas nen F2F2F2, kho A4 le 2 cm).
const fs = require('fs');
const path = require('path');
const {
  Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell, ImageRun,
  HeadingLevel, AlignmentType, WidthType, ShadingType, BorderStyle, LevelFormat, VerticalAlign,
} = require('docx');

const SRC = path.join(__dirname, 'report_C.md');
const OUT = path.join(__dirname, 'BaoCao_phanC_nhap.docx');
const TNR = 'Times New Roman';
const CONTENT_W = 9638;

const md = fs.readFileSync(SRC, 'utf8').replace(/^---[\s\S]*?---\n/, '');
const lines = md.split('\n');

function inline(text, base = {}) {
  const runs = [];
  const re = /(\*\*[^*]+\*\*|`[^`]+`)/g;
  let last = 0, m;
  while ((m = re.exec(text)) !== null) {
    if (m.index > last) runs.push(new TextRun({ text: text.slice(last, m.index), font: TNR, ...base }));
    const tok = m[0];
    if (tok.startsWith('**')) {
      runs.push(...inline(tok.slice(2, -2), { ...base, bold: true }));
    } else {
      runs.push(new TextRun({ text: tok.slice(1, -1), font: 'Consolas', size: base.size ? base.size - 2 : 22, ...(base.bold ? { bold: true } : {}) }));
    }
    last = m.index + tok.length;
  }
  if (last < text.length) runs.push(new TextRun({ text: text.slice(last), font: TNR, ...base }));
  return runs;
}

const body = (text, opts = {}) => new Paragraph({
  children: inline(text, opts.run || {}),
  alignment: opts.align || AlignmentType.JUSTIFIED,
  spacing: { before: 40, after: 100, line: 280 },
  ...(opts.numbering ? { numbering: opts.numbering } : {}),
});

function pngSize(file) {
  const b = fs.readFileSync(file);
  return { w: b.readUInt32BE(16), h: b.readUInt32BE(20) };
}

const cellBorder = { style: BorderStyle.SINGLE, size: 4, color: '808080' };
function makeTable(rows) {
  const ncol = rows[0].length;
  const len = Array(ncol).fill(4);
  rows.forEach(r => r.forEach((c, i) => { len[i] = Math.max(len[i], Math.min(c.replace(/[`*]/g, '').length, 60)); }));
  const tot = len.reduce((a, b) => a + b, 0);
  const sq = len.map(l => Math.sqrt(l)); const tsq = sq.reduce((a, b) => a + b, 0);
  const widths = sq.map(l => Math.max(1000, Math.round(CONTENT_W * l / tsq)));
  const scale = CONTENT_W / widths.reduce((a, b) => a + b, 0);
  for (let i = 0; i < ncol; i++) widths[i] = Math.round(widths[i] * scale);
  widths[ncol - 1] += CONTENT_W - widths.reduce((a, b) => a + b, 0);
  return new Table({
    width: { size: CONTENT_W, type: WidthType.DXA },
    columnWidths: widths,
    rows: rows.map((r, ri) => new TableRow({
      tableHeader: ri === 0,
      children: r.map((c, ci) => new TableCell({
        width: { size: widths[ci], type: WidthType.DXA },
        borders: { top: cellBorder, bottom: cellBorder, left: cellBorder, right: cellBorder },
        shading: ri === 0 ? { fill: 'D9E2F3', type: ShadingType.CLEAR, color: 'auto' } : undefined,
        margins: { top: 60, bottom: 60, left: 100, right: 100 },
        verticalAlign: VerticalAlign.CENTER,
        children: [new Paragraph({
          alignment: ri === 0 ? AlignmentType.CENTER : AlignmentType.LEFT,
          spacing: { before: 0, after: 0, line: 276 },
          children: inline(c, { size: 22, ...(ri === 0 ? { bold: true } : {}) }),
        })],
      })),
    })),
  });
}

const children = [];
let i = 0;
let numRef = 0;
const numberingConfigs = [{
  reference: 'bullets',
  levels: [{ level: 0, format: LevelFormat.BULLET, text: '•', alignment: AlignmentType.LEFT,
             style: { paragraph: { indent: { left: 567, hanging: 283 } } } },
           { level: 1, format: LevelFormat.BULLET, text: '–', alignment: AlignmentType.LEFT,
             style: { paragraph: { indent: { left: 1134, hanging: 283 } } } }],
}];
function newNumbered() {
  const ref = 'num' + (numRef++);
  numberingConfigs.push({ reference: ref, levels: [{ level: 0, format: LevelFormat.DECIMAL, text: '%1.',
    alignment: AlignmentType.LEFT, style: { paragraph: { indent: { left: 567, hanging: 340 } } } }] });
  return ref;
}
let curNum = null;

while (i < lines.length) {
  let line = lines[i];
  if (!/^\d+\. /.test(line) && !/^\s+- /.test(line) && line.trim() !== '') curNum = null;

  if (line.trim() === '') { i++; continue; }

  let m;
  if ((m = line.match(/^(#{1,3}) (.*)$/))) {
    const lvl = [HeadingLevel.HEADING_1, HeadingLevel.HEADING_2, HeadingLevel.HEADING_3][m[1].length - 1];
    children.push(new Paragraph({ heading: lvl, keepNext: true, children: [new TextRun({ text: m[2].replace(/`/g, ''), font: TNR })] }));
    i++; continue;
  }
  if (line.startsWith('> ')) {
    children.push(new Paragraph({
      children: inline(line.slice(2), { italics: true, color: '7F7F7F', size: 22 }),
      spacing: { before: 40, after: 160, line: 280 }, alignment: AlignmentType.JUSTIFIED,
    }));
    i++; continue;
  }
  if (line.startsWith('```')) {
    const code = [];
    i++;
    while (i < lines.length && !lines[i].startsWith('```')) code.push(lines[i++]);
    i++;
    code.forEach((c, k) => children.push(new Paragraph({
      keepNext: k < code.length - 1, keepLines: true,
      border: { left: { style: BorderStyle.SINGLE, size: 12, color: '1F3864', space: 6 } },
      shading: { fill: 'F2F2F2', type: ShadingType.CLEAR, color: 'auto' },
      spacing: { before: k === 0 ? 80 : 0, after: k === code.length - 1 ? 120 : 0, line: 252 },
      indent: { left: 200 },
      children: [new TextRun({ text: c.length ? c : ' ', font: 'Consolas', size: 19 })],
    })));
    continue;
  }
  if (line.startsWith('::: center')) {
    i++;
    while (i < lines.length && !lines[i].startsWith(':::')) {
      children.push(body(lines[i], { align: AlignmentType.CENTER, run: { italics: true } }));
      i++;
    }
    i++; continue;
  }
  if ((m = line.match(/^!\[(.*)\]\((.*?)\)(\{width=([\d.]+)cm\})?/))) {
    const file = path.join(__dirname, m[2]);
    const { w, h } = pngSize(file);
    const wpx = Math.round(parseFloat(m[4] || '15') * 37.8);
    const hpx = Math.round(wpx * h / w);
    children.push(new Paragraph({ alignment: AlignmentType.CENTER, keepNext: true, spacing: { before: 120, after: 0 },
      children: [new ImageRun({ type: 'png', data: fs.readFileSync(file), transformation: { width: wpx, height: hpx } })] }));
    children.push(new Paragraph({ alignment: AlignmentType.CENTER, spacing: { before: 80, after: 160 },
      children: [new TextRun({ text: m[1], font: TNR, italics: true, size: 22 })] }));
    i++; continue;
  }
  if (line.startsWith('|')) {
    const rows = [];
    while (i < lines.length && lines[i].startsWith('|')) {
      if (!/^\|[\s\-|:]+\|$/.test(lines[i])) {
        rows.push(lines[i].trim().slice(1, -1).split(' | ').map(s => s.trim().replace(/^\|\s*|\s*\|$/g, '')));
      }
      i++;
    }
    children.push(makeTable(rows));
    children.push(new Paragraph({ spacing: { before: 0, after: 80 }, children: [] }));
    continue;
  }
  if ((m = line.match(/^(\s*)- (.*)$/))) {
    children.push(body(m[2], { numbering: { reference: 'bullets', level: m[1].length >= 2 ? 1 : 0 } }));
    i++; continue;
  }
  if ((m = line.match(/^\d+\. (.*)$/))) {
    if (!curNum) curNum = newNumbered();
    children.push(body(m[1], { numbering: { reference: curNum, level: 0 } }));
    i++; continue;
  }
  // doan van thuong (gop cac dong lien tiep)
  let para = line;
  while (i + 1 < lines.length && lines[i + 1].trim() !== '' && !/^([#>|!`:]|\s*- |\d+\. )/.test(lines[i + 1])) {
    para += ' ' + lines[++i];
  }
  children.push(body(para));
  i++;
}

const doc = new Document({
  styles: {
    default: { document: { run: { font: TNR, size: 26 } } },
    paragraphStyles: [
      { id: 'Heading1', name: 'Heading 1', basedOn: 'Normal', next: 'Normal', quickFormat: true,
        run: { font: TNR, size: 32, bold: true, color: '2E74B5' },
        paragraph: { spacing: { before: 360, after: 160 }, keepNext: true, outlineLevel: 0 } },
      { id: 'Heading2', name: 'Heading 2', basedOn: 'Normal', next: 'Normal', quickFormat: true,
        run: { font: TNR, size: 28, bold: true, color: '2E74B5' },
        paragraph: { spacing: { before: 240, after: 120 }, keepNext: true, outlineLevel: 1 } },
      { id: 'Heading3', name: 'Heading 3', basedOn: 'Normal', next: 'Normal', quickFormat: true,
        run: { font: TNR, size: 26, bold: true, italics: true, color: '1F4D78' },
        paragraph: { spacing: { before: 200, after: 100 }, keepNext: true, outlineLevel: 2 } },
    ],
  },
  numbering: { config: numberingConfigs },
  sections: [{
    properties: { page: { size: { width: 11906, height: 16838 },
      margin: { top: 1134, right: 1134, bottom: 1134, left: 1134 } } },
    children: [
      new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 60 },
        children: [new TextRun({ text: 'BÁO CÁO BTL CO3009 – CÁC MỤC DO THÀNH VIÊN C VIẾT', font: TNR, bold: true, size: 30, color: '1F3864' })] }),
      new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 240 },
        children: [new TextRun({ text: 'Nháp v1 – để ghép vào Template_Report_BTL_CO3009.docx (A ghép bản cuối)', font: TNR, italics: true, size: 22 })] }),
      ...children,
    ],
  }],
});

Packer.toBuffer(doc).then(buf => { fs.writeFileSync(OUT, buf); console.log('wrote', OUT); });
