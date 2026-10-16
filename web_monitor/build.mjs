import { build, transform } from 'esbuild';
import { readFile, writeFile, mkdir, rm } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = dirname(fileURLToPath(import.meta.url));
const output = join(root, 'build');
const scriptTag = '<script type="module" src="app.mjs"></script>';
const stylesheetTag = '<link rel="stylesheet" href="styles.css">';

const [template, css, bundle] = await Promise.all([
  readFile(join(root, 'index.html'), 'utf8'),
  readFile(join(root, 'styles.css'), 'utf8'),
  build({
    entryPoints: [join(root, 'app.mjs')],
    bundle: true,
    format: 'iife',
    platform: 'browser',
    target: 'es2020',
    minify: true,
    write: false,
    legalComments: 'none'
  })
]);

if (template.split(scriptTag).length !== 2 || template.split(stylesheetTag).length !== 2
    || template.split('</body>').length !== 2) {
  throw new Error('The HTML entry tags changed; update build.mjs before deploying.');
}

const { code: compactCss } = await transform(css, { loader: 'css', minify: true });
// Prevent an embedded string from ending its surrounding HTML element.
const inlineJs = bundle.outputFiles[0].text.replace(/<\/script/gi, '<\\/script');
const inlineCss = compactCss.replace(/<\/style/gi, '\\3c /style');
const html = template
  .replace(stylesheetTag, () => `<style>${inlineCss}</style>`)
  .replace(scriptTag, '')
  // The source module is deferred; the inline classic bundle needs the DOM first.
  .replace('</body>', () => `<script>${inlineJs}</script>\n</body>`);

// Compile and validate before replacing the previous generated output.
await rm(output, { recursive: true, force: true });
await mkdir(output, { recursive: true });
await writeFile(join(output, 'index.html'), html);
console.log(`Built build/index.html (${Buffer.byteLength(html).toLocaleString()} bytes). No external JS or CSS requests.`);
