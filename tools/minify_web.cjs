// Conservative, parser-checked whitespace minification. No mangling or compression rewrites.
const fs = require('node:fs');
const assert = require('node:assert/strict');
const acorn = require('acorn');
const postcss = require('postcss');
assert.equal(acorn.version, '8.15.0');
assert.equal(require('postcss/package.json').version, '8.5.34');
const options = {ecmaVersion: 'latest', sourceType: 'script'};
const canonical = ast => JSON.stringify(ast, (key, value) => ['start','end','loc','range'].includes(key) ? undefined : typeof value === 'bigint' ? String(value)+'n' : value);
function js(source) {
  const tokens = [], comments = [];
  const original = acorn.parse(source, {...options, onToken: tokens, onComment: comments});
  // Preserve literal token text, all line terminators in gaps (ASI), and license comments.
  const pieces = [...tokens.filter(t => t.type.label !== 'eof'), ...comments.filter(c => /@license|@preserve|^!/.test(c.value))].sort((a,b) => a.start-b.start);
  let result = '', end = 0;
  for (const piece of pieces) {
    const gap = source.slice(end, piece.start);
    if (/[\r\n\u2028\u2029]/.test(gap)) result += '\n';
    else if (gap.length && !/[(){}\[\],:;]/.test(result.slice(-1)) && !/[(){}\[\],:;]/.test(source[piece.start])) result += ' ';
    result += source.slice(piece.start, piece.end);
    end = piece.end;
  }
  result += '\n';
  assert.equal(canonical(acorn.parse(result, options)), canonical(original), 'JS AST changed');
  return result;
}
function cssTree(node) {
  return [node.type, node.selector, node.name, node.params, node.prop, node.value, node.important, node.text, node.nodes?.map(cssTree)];
}
function css(source) {
  const root = postcss.parse(source);
  const original = cssTree(root);
  root.walk(node => {
    node.raws.before = '';
    if (node.type === 'decl' && !node.prop.startsWith('--')) node.raws.between = ':';
    if (node.nodes) node.raws.after = '';
    // Preserve selector, declaration values (especially custom properties), and comments.
    if (node.type === 'rule' || (node.type === 'atrule' && node.nodes)) node.raws.between = '';
  });
  root.raws.after = '';
  const result = root.toString();
  assert.deepEqual(cssTree(postcss.parse(result)), original, 'CSS tree changed');
  return result;
}
for (const [file, fn] of [[process.argv[2], js], [process.argv[3], css]]) {
  const source = fs.readFileSync(file, 'utf8');
  const result = fn(source);
  assert.ok(Buffer.byteLength(result) <= Buffer.byteLength(source), 'Minification grew asset');
  fs.writeFileSync(file, result, 'utf8');
  console.log(`${file}: ${Buffer.byteLength(source)} -> ${Buffer.byteLength(result)} bytes`);
}
