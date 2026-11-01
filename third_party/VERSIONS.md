# Third-party dependencies (pinned)

Every dependency is vendored or locked here so the project builds reproducibly and offline.
Update this table in the same commit that changes a dependency.

| Component | Version | Tag / Commit | Source | Licence | Where used | Status |
|---|---|---|---|---|---|---|
| Catch2 (amalgamated) | v3.16.1 | tag commit 0809213 (2026-10-08) | https://github.com/catchorg/Catch2 | BSL-1.0 | tests only | vendored |
| cpp-httplib | v0.64.0 | commit 24e58ce5 (2026-10-29) | https://github.com/yhirose/cpp-httplib | MIT | src/server | vendored |
| nlohmann/json | v3.12.0 | tag sha 65ee684, commit 55f93686 (2025-04-11) | https://github.com/nlohmann/json | MIT | src/server | vendored |
| Microsoft.Web.WebView2 SDK | 1.0.4314.35 | NuGet package (2026-10-23) | https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.4314.35 | Microsoft SDK Licence (see third_party/webview2/LICENSE.txt) | src/host (Windows only) | vendored (headers + x64 static loader lib) |
| React | 18.3.1 | release 2024-04-26 | npm:react | MIT | src/ui frontend runtime | locked by package-lock.json |
| React DOM | 18.3.1 | release 2024-04-26 | npm:react-dom | MIT | src/ui DOM renderer | locked by package-lock.json |
| Three.js | 0.169.0 | release 2024-09-27 | npm:three | MIT | src/ui 3D mesh & point cloud | locked by package-lock.json |
| Plotly.js (Cartesian Dist) | 2.35.2 | release 2024-09-17 | npm:plotly.js-cartesian-dist-min | MIT | src/ui 2D thermal plots | locked by package-lock.json |
| Framer Motion | 11.11.11 | release 2024-10-18 | npm:framer-motion | MIT | src/ui view transitions only | locked by package-lock.json |
| IBM Plex Sans | 5.1.0 | release 2024-06-25 | npm:@fontsource/ibm-plex-sans | OFL-1.1 | src/ui typography (self-hosted) | locked by package-lock.json |
| IBM Plex Mono | 5.1.0 | release 2024-06-25 | npm:@fontsource/ibm-plex-mono | OFL-1.1 | src/ui measurements & tabular nums | locked by package-lock.json |
| Tailwind CSS | 3.4.14 | release 2024-10-15 | npm:tailwindcss | MIT | src/ui styling & design tokens | locked by package-lock.json |
| PostCSS | 8.4.47 | release 2024-09-19 | npm:postcss | MIT | src/ui CSS build pipeline | locked by package-lock.json |
| Autoprefixer | 10.4.20 | release 2024-08-06 | npm:autoprefixer | MIT | src/ui vendor prefixing | locked by package-lock.json |
| TypeScript | 5.6.3 | release 2024-10-09 | npm:typescript | Apache-2.0 | src/ui typechecking | locked by package-lock.json |
| Vite | 5.4.10 | release 2024-10-23 | npm:vite | MIT | src/ui bundler & build tool | locked by package-lock.json |
| @vitejs/plugin-react | 4.3.3 | release 2024-10-15 | npm:@vitejs/plugin-react | MIT | src/ui React Fast Refresh & JSX | locked by package-lock.json |
| Vitest | 2.1.4 | release 2024-10-29 | npm:vitest | MIT | src/ui unit test runner | locked by package-lock.json |
| Playwright | 1.48.2 | release 2024-10-24 | npm:playwright | Apache-2.0 | src/ui E2E & visual verification | locked by package-lock.json |
| @types/react | 18.3.12 | release 2024-10-22 | npm:@types/react | MIT | src/ui TypeScript definitions | locked by package-lock.json |
| @types/react-dom | 18.3.1 | release 2024-10-15 | npm:@types/react-dom | MIT | src/ui TypeScript definitions | locked by package-lock.json |
| @types/three | 0.169.0 | release 2024-09-30 | npm:@types/three | MIT | src/ui TypeScript definitions | locked by package-lock.json |
| @types/node | 22.8.1 | release 2024-10-24 | npm:@types/node | MIT | src/ui build scripting types | locked by package-lock.json |
| @axe-core/playwright | 4.10.0 | release 2024-09-04 | npm:@axe-core/playwright | MPL-2.0 | src/ui automated accessibility auditing | locked by package-lock.json |

Selection rule: pick a release at least two weeks old (verified above for all Phase 1 deps),
check recent commit activity, record tag name and commit hash.