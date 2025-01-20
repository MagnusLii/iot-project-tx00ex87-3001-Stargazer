function openModal(image) {
    document.getElementById("image-modal").style.display = "block";
    document.getElementById("image-modal-content").src = image;
}

function closeModal() {
    document.getElementById("image-modal").style.display = "none";
}

document.addEventListener("DOMContentLoaded", () => {
    const imgs = document.querySelectorAll(".photo");
    imgs.forEach(img => {
        img.onclick = () => openModal(img.src);
    });

    document.getElementById("image-modal-close").onclick = () => closeModal();
})
