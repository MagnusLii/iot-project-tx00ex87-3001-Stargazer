function filterDiagnostics() {
    const filter = document.getElementById("diagnostics-filter").value;
    if (filter == "") {
        window.location.href = "/control/diagnostics";
    } else {
        window.location.href = "/control/diagnostics?name=" + filter;
    }
}
