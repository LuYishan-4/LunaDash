"use strict";
const accents = document.querySelectorAll("[data-color]");
accents.forEach(button => button.addEventListener("click", () => {
  document.documentElement.style.setProperty("--accent", button.dataset.color);
  accents.forEach(item => item.setAttribute("aria-pressed", String(item === button)));
}));
const gap = document.getElementById("gap");
gap.addEventListener("input", () => {
  document.documentElement.style.setProperty("--gap", `${gap.value}px`);
  document.getElementById("gap-value").textContent = `${gap.value} px`;
});
document.getElementById("copy").addEventListener("click", async () => {
  const status = document.getElementById("copy-status");
  try {
    await navigator.clipboard.writeText(document.getElementById("commands").textContent);
    status.textContent = "Commands copied.";
  } catch {
    status.textContent = "Select the commands above to copy them manually.";
  }
});
