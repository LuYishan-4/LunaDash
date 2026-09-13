import { cp, copyFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
const site = new URL("../", import.meta.url);
for (const name of ["index.html", "styles.css"]) {
  await copyFile(new URL(name, site), new URL(`dist/${name}`, site));
}
await cp(fileURLToPath(new URL("assets", site)), fileURLToPath(new URL("dist/assets", site)), { recursive: true });
