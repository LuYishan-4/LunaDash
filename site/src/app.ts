function requireElement<T extends HTMLElement>(id: string, constructor: { new(): T }): T {
  const element = document.getElementById(id);
  if (!(element instanceof constructor)) throw new Error(`Missing element: ${id}`);
  return element;
}

const accents = document.querySelectorAll<HTMLButtonElement>("button[data-color]");
accents.forEach(button => button.addEventListener("click", () => {
  const color = button.dataset.color;
  if (!color || !/^#[0-9a-f]{6}$/i.test(color)) return;
  document.documentElement.style.setProperty("--accent", color);
  accents.forEach(item => item.setAttribute("aria-pressed", String(item === button)));
}));

const gap = requireElement("gap", HTMLInputElement);
const gapValue = requireElement("gap-value", HTMLOutputElement);
gap.addEventListener("input", () => {
  const value = gap.valueAsNumber;
  if (!Number.isFinite(value) || value < 4 || value > 32) return;
  document.documentElement.style.setProperty("--gap", `${value}px`);
  gapValue.textContent = `${value} px`;
});

const copy = requireElement("copy", HTMLButtonElement);
const status = requireElement("copy-status", HTMLSpanElement);
const commands = requireElement("commands", HTMLElement);
copy.addEventListener("click", async () => {
  try {
    await navigator.clipboard.writeText(commands.textContent ?? "");
    status.textContent = "Commands copied.";
  } catch {
    status.textContent = "Select the commands above to copy them manually.";
  }
});

export {};
