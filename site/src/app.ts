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

const galleryDialog = document.querySelector<HTMLDialogElement>(".image-dialog");
const galleryImage = galleryDialog?.querySelector<HTMLImageElement>(".image-dialog-image");
const galleryTitle = galleryDialog?.querySelector<HTMLElement>("#image-dialog-title");
const galleryOriginal = galleryDialog?.querySelector<HTMLAnchorElement>(".image-dialog-original");
let galleryTrigger: HTMLAnchorElement | null = null;

if (galleryDialog && galleryImage && galleryTitle && galleryOriginal) {
  document.querySelectorAll<HTMLAnchorElement>("a[data-lightbox]").forEach(link => {
    link.addEventListener("click", event => {
      // Keep normal link behavior for new tabs and browsers without dialogs.
      if (event.button !== 0 || event.ctrlKey || event.metaKey || event.shiftKey || event.altKey || typeof galleryDialog.showModal !== "function") return;
      event.preventDefault();
      galleryTrigger = link;
      galleryImage.src = link.href;
      galleryImage.alt = link.querySelector("img")?.alt ?? "LunaDash desktop";
      galleryTitle.textContent = link.dataset.title ?? "Desktop screenshot";
      galleryOriginal.href = link.href;
      galleryDialog.showModal();
    });
  });
  galleryDialog.querySelector(".image-dialog-close")?.addEventListener("click", () => galleryDialog.close());
  galleryDialog.addEventListener("click", event => {
    if (event.target !== galleryDialog) return;
    const bounds = galleryDialog.getBoundingClientRect();
    if (event.clientX < bounds.left || event.clientX > bounds.right || event.clientY < bounds.top || event.clientY > bounds.bottom) galleryDialog.close();
  });
  galleryDialog.addEventListener("close", () => galleryTrigger?.focus());
}

export {};
