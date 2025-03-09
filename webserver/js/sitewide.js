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

function navResponsive() {
    const nav = document.getElementById("nav");
    if (nav.className === "topnav") {
        nav.className += " responsive";
    } else {
        nav.className = "topnav";
    }
}

document.addEventListener("DOMContentLoaded", () => {
    currentUser();
});
