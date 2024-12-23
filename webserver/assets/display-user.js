"use strict";

function currentUser() {
    fetch(`/users/current`, { method: "POST" }).then(response => response.text()).then(data => {
        const user = JSON.parse(data);
        document.querySelectorAll("#user").forEach(element => {
            element.innerHTML = user.username;
        })
    }).catch(() => {
        document.querySelectorAll("#user").forEach(element => {
            element.innerHTML = "N/A";
        })
    });
}

document.addEventListener("DOMContentLoaded", () => {
    currentUser();
});
