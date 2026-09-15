// Dismiss the boot overlay after its exit animation. The overlay also ends
// hidden through animation-fill-mode, so this only removes the leftover node
// and never gates the page content.
const boot = document.getElementById("boot");
if (boot) {
  const dismiss = (): void => boot.remove();
  boot.addEventListener("animationend", (event) => {
    if (event.animationName === "boot-out") dismiss();
  });
  // Safety net: a stalled or interrupted animation must not leave it behind.
  window.setTimeout(dismiss, 4000);
}

export {};
