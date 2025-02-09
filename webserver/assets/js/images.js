function openModal(image, caption) {
    console.log(image, caption);
    document.getElementById("image-modal").style.display = "block";
    document.getElementById("image-modal-image").src = image;
    document.getElementById("image-modal-caption").innerHTML = caption;
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
    let page_size = Number(document.getElementById("gallery-page-size-select").value);
    if (page_size == 0) {
        page_size = 8;
    }

    window.location.href = `/gallery?psize=${page_size}`
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
    localStorage.setItem("view", view);
}

function disableButtons() {
    const page = Number(document.getElementById("gallery-current").dataset.page);
    const pages = Number(document.getElementById("gallery-current").dataset.pages);

    if (page <= 1) {
        document.getElementById("gallery-previous").disabled = true;
    }
    if (page >= pages) {
        document.getElementById("gallery-next").disabled = true;
    }
}

document.addEventListener("DOMContentLoaded", () => {
    const view = localStorage.getItem("view");
    if (view) {
        document.getElementById("gallery-view-select").value = view;
        changeView();
    }

    document.addEventListener("click", (event) => {
        if (document.getElementById("image-modal").style.display === "none") {
            if (event.target.matches(".photo")) {
                openModal(event.target.src, event.target.alt);
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
