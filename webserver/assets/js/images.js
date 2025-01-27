function openModal(image) {
    document.getElementById("image-modal").style.display = "block";
    document.getElementById("image-modal-content").src = image;
}

function closeModal() {
    document.getElementById("image-modal").style.display = "none";
}

function prevPage() {
    const query_params = new URLSearchParams(window.location.search);
    const page = Number(query_params.get("page"));
    let page_size = Number(query_params.get("psize"));
    if (page <= 1) {
        return
    }
    if (page_size == 0) {
        page_size = 8;
    }

    window.location.href = `/gallery?page=${page - 1}&psize=${page_size}`
}

function nextPage() {
    const query_params = new URLSearchParams(window.location.search);
    let page = Number(query_params.get("page"));
    let page_size = Number(query_params.get("psize"));
    if (page == 0) {
        page = 1;
    }
    if (page_size == 0) {
        page_size = 8;
    }

    window.location.href = `/gallery?page=${page + 1}&psize=${page_size}`
}

function changePageSize() {
    const query_params = new URLSearchParams(window.location.search);
    let current = Number(query_params.get("page"));
    let page_size = Number(document.getElementById("gallery-page-size-select").value);
    if (page_size == 0) {
        page_size = 8;
    }

    window.location.href = `/gallery?page=${current}&psize=${page_size}`
}

function disableButtons() {
    const query_params = new URLSearchParams(window.location.search);
    const page = Number(query_params.get("page"));
    const pages = Number(document.getElementById("gallery-current").dataset.page);

    if (page <= 1) {
        document.getElementById("gallery-previous").disabled = true;
    }
    if (page >= pages) {
        document.getElementById("gallery-next").disabled = true;
    }
}

document.addEventListener("DOMContentLoaded", () => {
    const imgs = document.querySelectorAll(".photo");
    imgs.forEach(img => {
        img.onclick = () => openModal(img.src);
    });

    document.getElementById("image-modal-close").onclick = () => closeModal();

    disableButtons();
})
