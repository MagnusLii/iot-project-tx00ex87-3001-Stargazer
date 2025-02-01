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

function changeView() {
    const view = Number(document.getElementById("gallery-view-select").value);
    if (view == 1) {
        document.getElementById("gallery-container").className = "view-grid-small";
    } else if (view == 2) {
        document.getElementById("gallery-container").className = "view-grid-medium";
    } else if (view == 3) {
        document.getElementById("gallery-container").className = "view-grid-large";
    } else if (view == 4) {
        document.getElementById("gallery-container").className = "view-grid-full";
    }
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
    document.addEventListener("click", (event) => {
        if (document.getElementById("image-modal").style.display === "none") {
            if (event.target.matches(".photo")) {
                openModal(event.target.src);
            }
        } else {
            if (event.target.matches("#image-modal-close") ||
                !event.target.closest("#image-modal-content")) {
                closeModal();
            }
        }

        false
    });

    disableButtons();
})
