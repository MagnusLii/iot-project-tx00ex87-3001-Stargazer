function filterDiagnostics() {
    const filter = document.getElementById("diagnostics-filter").value;
    const filter_status = document.getElementById("diagnostics-filter-status").value;
    if (filter == "" && filter_status == "") {
        window.location.href = "/control/diagnostics";
    } else if (filter_status == "") {
        window.location.href = "/control/diagnostics?name=" + filter;
    } else if (filter == "") {
        window.location.href = "/control/diagnostics?status=" + filter_status;
    } else {
        window.location.href = "/control/diagnostics?name=" + filter + "&status=" + filter_status;
    }
}

function nextPage() {
    const query_params = new URLSearchParams(window.location.search);
    let page = Number(document.getElementById("page_count").dataset.page);
    let pages = Number(document.getElementById("page_count").dataset.pages);

    if (page == pages) {
        return;
    } else if (page > pages) {
        page = pages;
    }

    if (query_params.has("name")) {
        window.location.href = `/control/diagnostics?name=${query_params.get("name")}&page=${page + 1}`
    } else {
        window.location.href = `/control/diagnostics?page=${page + 1}`
    }
}

function prevPage() {
    const query_params = new URLSearchParams(window.location.search);
    let page = Number(document.getElementById("page_count").dataset.page);

    if (page == 1) {
        return;
    } else if (page <= 0) {
        page = 1;
    }
    if (query_params.has("name")) {
        window.location.href = `/control/diagnostics?name=${query_params.get("name")}&page=${page - 1}`
    } else {
        window.location.href = `/control/diagnostics?page=${page - 1}`
    }
}
