const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');
const marked = require('marked');
const katex = require('katex');

const repoRoot = path.resolve(__dirname, '..');
const reportDir = path.join(repoRoot, 'report');

const trackFiles = [
    { file: '00_intro.md', num: '0', title: 'Introduction, Hardware Environment & System Specifications' },
    { file: 'A_continuous.md', num: '1', title: 'Track A — Continuous Optimization & Loss Landscapes' },
    { file: 'B_discrete.md', num: '2', title: 'Track B — Discrete Combinatorial Algorithms & QUBO / Ising Mappings' },
    { file: 'C_quantum.md', num: '3', title: 'Track C — Quantum Variational Optimization (QAOA, VQE, QNN & Noise)' },
    { file: 'D_submodular_parallel.md', num: '4', title: 'Track D — Submodular Coverage, Multicore Scaling & Federated Learning' },
    { file: 'E_case_study.md', num: '5', title: 'Track E — Real-World Case Study: EV-VRPTW Fleet Logistics' },
    { file: 'F_compiler.md', num: '6', title: 'Track F — Compiler Intermediate Representation Passes & Register Allocation' },
    { file: 'G_xv6.md', num: '7', title: 'Track G — xv6 RISC-V Kernel Scheduling & Copy-on-Write Virtual Memory' },
    { file: 'H_dbms.md', num: '8', title: 'Track H — PostgreSQL TPC-H SF 0.1 Join Ordering & Knapsack Index Selection' },
    { file: 'I_quantum_algorithms.md', num: '9', title: 'Track I — Quantum Algorithms, Oracles, Grover Scaling & QML vs Classical SVM' },
    { file: 'J_softeng.md', num: '10', title: 'Track J — Software Engineering Test-Suite Minimization on pugixml' },
    { file: '99_conclusion.md', num: '11', title: 'Synthesis, Honest Limitations & Section 11 Conclusions' }
];

// Helper to escape regex
function escapeRegExp(string) {
    return string.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

// Render KaTeX in text
function renderMath(text) {
    // Replace display math $$...$$
    text = text.replace(/\$\$([\s\S]+?)\$\$/g, (match, formula) => {
        try {
            return `<div class="katex-display">${katex.renderToString(formula.trim(), { displayMode: true, throwOnError: false })}</div>`;
        } catch (e) {
            return match;
        }
    });

    // Replace inline math $...$
    // Ensure we don't match escaped dollars \$ or currency numbers like $1,170 or $80
    // Negative lookbehind for backslash, and ensure content doesn't start or end with space
    text = text.replace(/(?<!\\)\$([^\$\n]+?)(?<!\\)\$/g, (match, formula) => {
        // If it looks like currency ($100, $1,170.83), skip
        if (/^\s*[\d,.]+(\s*(?:km|kwh|hr|mb|ms|kg|h))?\s*$/i.test(formula)) {
            return match;
        }
        try {
            return katex.renderToString(formula.trim(), { displayMode: false, throwOnError: false });
        } catch (e) {
            return match;
        }
    });

    return text;
}

// Convert relative image path to absolute file URL
function fixImagePaths(mdContent, sourceFilePath) {
    const sourceDir = path.dirname(sourceFilePath);
    return mdContent.replace(/!\[([^\]]*)\]\(([^)]+)\)/g, (match, alt, imgPath) => {
        let absPath;
        if (imgPath.startsWith('file:///')) {
            absPath = imgPath.replace(/^file:\/\/\/?/, '');
        } else if (path.isAbsolute(imgPath)) {
            absPath = imgPath;
        } else {
            absPath = path.resolve(sourceDir, imgPath);
        }
        absPath = absPath.replace(/\\/g, '/');
        if (!absPath.startsWith('/')) {
            absPath = '/' + absPath;
        }
        return `![${alt}](file://${absPath})`;
    });
}

// Main compiler
function buildReport() {
    console.log("Reading track markdown files...");
    let fullSectionsHtml = [];
    let tocEntries = [];

    // KaTeX CSS content
    const katexCssPath = path.join(repoRoot, 'node_modules', 'katex', 'dist', 'katex.min.css');
    let katexCss = '';
    if (fs.existsSync(katexCssPath)) {
        katexCss = fs.readFileSync(katexCssPath, 'utf8');
        // Fix font URLs in katex.min.css to absolute file URLs
        const katexFontsDir = path.join(repoRoot, 'node_modules', 'katex', 'dist', 'fonts').replace(/\\/g, '/');
        katexCss = katexCss.replace(/url\(fonts\//g, `url(file:///${katexFontsDir}/`);
    }

    trackFiles.forEach((item, index) => {
        const filePath = path.join(reportDir, item.file);
        if (!fs.existsSync(filePath)) {
            console.error(`File missing: ${filePath}`);
            return;
        }
        let rawMd = fs.readFileSync(filePath, 'utf8');

        // Fix image paths
        rawMd = fixImagePaths(rawMd, filePath);

        // Pre-render KaTeX math
        let mdWithMath = renderMath(rawMd);

        // Convert to HTML via marked
        let html = marked.parse(mdWithMath);

        const sectionId = `sec-${item.num}`;
        tocEntries.push({ num: item.num, title: item.title, id: sectionId });

        fullSectionsHtml.push(`
            <section class="report-section" id="${sectionId}">
                <div class="section-badge">Section ${item.num}</div>
                ${html}
            </section>
            <div class="page-break"></div>
        `);
    });

    // Build Table of Contents HTML
    let tocHtml = `
        <div class="toc-container">
            <h2 class="toc-header">Table of Contents</h2>
            <div class="toc-grid">
                ${tocEntries.map(e => `
                    <div class="toc-row">
                        <span class="toc-num">Section ${e.num}</span>
                        <span class="toc-title"><a href="#${e.id}">${e.title}</a></span>
                        <span class="toc-dots"></span>
                    </div>
                `).join('')}
            </div>
        </div>
        <div class="page-break"></div>
    `;

    // Title Page HTML
    const titlePageHtml = `
        <div class="title-page">
            <div class="title-top">
                <div class="univ-header">OPTIMIZATION TECHNIQUES | LAB ASSIGNMENT 8</div>
                <div class="course-sub">Department of Computer Science & Engineering</div>
            </div>
            
            <div class="title-main">
                <h1 class="main-title">OPTIMIZE ANYTHING</h1>
                <h2 class="main-subtitle">A 14-Day Vibe-Coding Challenge Across 44 Prescribed Curriculum Topics</h2>
                <div class="main-desc">
                    Comprehensive Empirical Report Spanning Continuous Optimization, Combinatorial QUBO / Ising Systems,
                    Variational Quantum Circuits, Multicore & Federated Learning, Real-World Last-Mile Logistics,
                    Compiler Instruction Passes, RISC-V Operating System Kernels, Database Engines,
                    and Software Engineering Test-Suite Minimization.
                </div>
            </div>

            <div class="title-meta-box">
                <table class="meta-table">
                    <tr><td><strong>Student Name</strong></td><td>Shreyansh Mojidra</td></tr>
                    <tr><td><strong>Roll Number</strong></td><td>241070045</td></tr>
                    <tr><td><strong>Course Title</strong></td><td>Optimization Techniques</td></tr>
                    <tr><td><strong>Assignment</strong></td><td>Assignment 8: "Optimize Anything"</td></tr>
                    <tr><td><strong>Submission Date</strong></td><td>October 28, 2026</td></tr>
                    <tr><td><strong>Primary AI Pair</strong></td><td>Antigravity (Gemini 3.8 Flash)</td></tr>
                    <tr><td><strong>Comparative AI Tool</strong></td><td>Claude 3.5 Sonnet</td></tr>
                    <tr><td><strong>Execution Platform</strong></td><td>Intel Core i3-1005G1 CPU @ 1.20GHz (2 Cores, 4 Threads), 8GB RAM, Windows 11 Home & WSL2 Ubuntu 24.04 LTS (Kernel 6.6.87)</td></tr>
                </table>
            </div>

            <div class="title-footer">
                <div>All claims strictly backed by reproducible logs in repository catalogs.</div>
                <div>Simulated models, theoretical derivations, and physical hardware executions explicitly labeled.</div>
            </div>
        </div>
        <div class="page-break"></div>
    `;

    // Complete HTML template
    const fullHtml = `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>Optimization Techniques Assignment 8 — Final Technical Report</title>
    <style>
        ${katexCss}

        @page {
            size: A4 portrait;
            margin: 20mm 15mm 20mm 15mm;
            @top-right {
                content: "Optimization Techniques — Assignment 8";
                font-family: 'Segoe UI', Arial, sans-serif;
                font-size: 8pt;
                color: #7f8c8d;
            }
            @bottom-center {
                content: "Page " counter(page);
                font-family: 'Segoe UI', Arial, sans-serif;
                font-size: 8.5pt;
                color: #555555;
            }
        }

        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            font-size: 9.5pt;
            line-height: 1.55;
            color: #2c3e50;
            background-color: #ffffff;
            margin: 0;
            padding: 0;
        }

        .page-break {
            page-break-after: always;
            break-after: page;
        }

        /* Title Page */
        .title-page {
            display: flex;
            flex-direction: column;
            justify-content: space-between;
            min-height: 240mm;
            box-sizing: border-box;
            padding: 20mm 10mm 10mm 10mm;
            text-align: center;
        }
        .title-top {
            border-bottom: 2px solid #2980b9;
            padding-bottom: 15px;
        }
        .univ-header {
            font-size: 13pt;
            font-weight: 700;
            letter-spacing: 2px;
            color: #2980b9;
        }
        .course-sub {
            font-size: 10pt;
            color: #7f8c8d;
            margin-top: 5px;
        }
        .title-main {
            margin: 35mm 0 25mm 0;
        }
        .main-title {
            font-size: 28pt;
            font-weight: 900;
            color: #1a252f;
            letter-spacing: 1px;
            margin: 0 0 10px 0;
        }
        .main-subtitle {
            font-size: 13pt;
            font-weight: 600;
            color: #34495e;
            margin: 0 0 20px 0;
        }
        .main-desc {
            font-size: 9.5pt;
            color: #555;
            max-width: 150mm;
            margin: 0 auto;
            line-height: 1.6;
        }
        .title-meta-box {
            background-color: #f8f9fa;
            border: 1px solid #e1e8ed;
            border-radius: 6px;
            padding: 15px 25px;
            margin: 10mm auto;
            width: 85%;
            text-align: left;
        }
        .meta-table {
            width: 100%;
            border-collapse: collapse;
        }
        .meta-table td {
            padding: 5px 10px;
            font-size: 9pt;
            border-bottom: 1px solid #ecf0f1;
        }
        .meta-table tr:last-child td {
            border-bottom: none;
        }
        .meta-table td:first-child {
            width: 38%;
            color: #34495e;
        }
        .meta-table td:last-child {
            color: #111;
        }
        .title-footer {
            font-size: 8pt;
            color: #95a5a6;
            line-height: 1.6;
            margin-top: 20mm;
            border-top: 1px solid #ecf0f1;
            padding-top: 10px;
        }

        /* Table of Contents */
        .toc-container {
            padding: 10mm 5mm;
        }
        .toc-header {
            font-size: 18pt;
            font-weight: 800;
            color: #1a252f;
            border-bottom: 3px solid #2980b9;
            padding-bottom: 8px;
            margin-bottom: 20px;
        }
        .toc-grid {
            display: flex;
            flex-direction: column;
            gap: 10px;
        }
        .toc-row {
            display: flex;
            align-items: baseline;
            font-size: 10pt;
        }
        .toc-num {
            font-weight: 700;
            color: #2980b9;
            width: 85px;
            flex-shrink: 0;
        }
        .toc-title a {
            color: #2c3e50;
            text-decoration: none;
            font-weight: 600;
        }
        .toc-title a:hover {
            color: #2980b9;
        }
        .toc-dots {
            flex-grow: 1;
            border-bottom: 1px dotted #bdc3c7;
            margin: 0 10px;
        }

        /* Section Styling */
        .report-section {
            padding-top: 5mm;
        }
        .section-badge {
            display: inline-block;
            background: #2980b9;
            color: #ffffff;
            font-size: 8pt;
            font-weight: 700;
            text-transform: uppercase;
            padding: 3px 10px;
            border-radius: 4px;
            margin-bottom: 10px;
        }

        h1 {
            font-size: 17pt;
            font-weight: 800;
            color: #1a252f;
            border-bottom: 2px solid #2980b9;
            padding-bottom: 6px;
            margin-top: 0;
            margin-bottom: 14px;
            page-break-after: avoid;
        }
        h2 {
            font-size: 13pt;
            font-weight: 700;
            color: #2c3e50;
            margin-top: 20px;
            margin-bottom: 10px;
            border-left: 4px solid #2980b9;
            padding-left: 8px;
            page-break-after: avoid;
        }
        h3 {
            font-size: 11pt;
            font-weight: 700;
            color: #34495e;
            margin-top: 16px;
            margin-bottom: 8px;
            page-break-after: avoid;
        }
        h4 {
            font-size: 9.5pt;
            font-weight: 700;
            color: #4b6584;
            margin-top: 12px;
            margin-bottom: 6px;
            page-break-after: avoid;
        }

        p {
            margin: 0 0 10px 0;
            text-align: justify;
        }

        /* Tables */
        table {
            width: 100%;
            border-collapse: collapse;
            font-size: 8.5pt;
            margin: 14px 0;
            page-break-inside: avoid;
            background-color: #ffffff;
        }
        th, td {
            border: 1px solid #dcdde1;
            padding: 6px 9px;
            text-align: left;
        }
        th {
            background-color: #2c3e50;
            color: #ffffff;
            font-weight: 700;
        }
        tr:nth-child(even) td {
            background-color: #f8f9fa;
        }
        tr:hover td {
            background-color: #edf2f7;
        }

        /* Images & Figures */
        img {
            max-width: 95%;
            height: auto;
            display: block;
            margin: 12px auto;
            border: 1px solid #e1e8ed;
            border-radius: 4px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.05);
            page-break-inside: avoid;
        }
        figure {
            margin: 14px 0;
            text-align: center;
            page-break-inside: avoid;
        }
        figcaption {
            font-size: 8.5pt;
            color: #7f8c8d;
            margin-top: 6px;
            font-style: italic;
        }

        /* Code Blocks */
        pre {
            background-color: #f4f6f7;
            border: 1px solid #e1e8ed;
            border-left: 3px solid #2980b9;
            padding: 10px;
            font-size: 8pt;
            font-family: "Consolas", "Monaco", "Courier New", monospace;
            overflow-x: auto;
            border-radius: 3px;
            margin: 10px 0;
            page-break-inside: avoid;
        }
        code {
            font-family: "Consolas", "Monaco", "Courier New", monospace;
            font-size: 8.5pt;
            background-color: #f1f2f6;
            padding: 1px 4px;
            border-radius: 3px;
            color: #c0392b;
        }
        pre code {
            background-color: transparent;
            padding: 0;
            color: #2c3e50;
        }

        /* Blockquotes / Callouts */
        blockquote {
            background-color: #f7f9fa;
            border-left: 4px solid #3498db;
            margin: 12px 0;
            padding: 8px 14px;
            color: #4a6572;
            font-style: italic;
        }

        ul, ol {
            margin: 0 0 10px 22px;
            padding: 0;
        }
        li {
            margin-bottom: 4px;
        }

        a {
            color: #2980b9;
            text-decoration: none;
        }
        a:hover {
            text-decoration: underline;
        }

        /* KaTeX tweaks */
        .katex {
            font-size: 1.02em;
        }
        .katex-display {
            margin: 8px 0;
            overflow-x: auto;
            overflow-y: hidden;
            page-break-inside: avoid;
        }

        /* Status tags */
        .status-badge {
            font-size: 7.5pt;
            font-weight: 700;
            padding: 2px 6px;
            border-radius: 3px;
            display: inline-block;
        }
        .status-hw {
            background-color: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .status-sim {
            background-color: #fff3cd;
            color: #856404;
            border: 1px solid #ffeeba;
        }
        .status-theo {
            background-color: #d1ecf1;
            color: #0c5460;
            border: 1px solid #bee5eb;
        }
    </style>
</head>
<body>
    ${titlePageHtml}
    ${tocHtml}
    ${fullSectionsHtml.join('\n')}
</body>
</html>`;

    const htmlOutputPath = path.join(reportDir, 'full_technical_report.html');
    fs.writeFileSync(htmlOutputPath, fullHtml, 'utf8');
    console.log(`Generated complete HTML: ${htmlOutputPath} (${(fullHtml.length / 1024).toFixed(1)} KB)`);

    // Now invoke Microsoft Edge headless to print to PDF
    const pdfOutputPath = path.join(repoRoot, 'FINAL_TECHNICAL_REPORT.pdf');
    const edgePath = "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe";
    
    console.log(`Generating PDF via Microsoft Edge Headless into ${pdfOutputPath}...`);
    const cmd = `"${edgePath}" --headless --disable-gpu --run-all-compositor-stages-before-draw --no-pdf-header-footer --print-to-pdf="${pdfOutputPath}" "file:///${htmlOutputPath.replace(/\\/g, '/')}"`;
    
    execSync(cmd, { stdio: 'inherit' });
    
    if (fs.existsSync(pdfOutputPath)) {
        const stats = fs.statSync(pdfOutputPath);
        console.log(`SUCCESS! Generated ${pdfOutputPath} (${(stats.size / 1024).toFixed(1)} KB)`);
    } else {
        console.error(`FAILED to generate ${pdfOutputPath}`);
    }
}

buildReport();
