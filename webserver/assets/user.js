"use strict";

function currentUser() {
    fetch(`/users/current`, { method: "POST" }).then(response => response.text()).then(data => {
        document.getElementById("user").innerHTML = data;
    }).catch(() => {
        document.getElementById("user").innerHTML = "N/A";
    });
}

function addUser() {
    fetch(`/users`, {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({
            "username": document.getElementById("name").value,
            "password": document.getElementById("password").value
        })
    }).then(response => response.text()).then(data => {
        document.getElementById("message").innerHTML = data;
    }).catch(error => {
        alert(error);
    });
}

function deleteUser(id) {
    if (confirm("Are you sure you want to delete this user?") == true) {
        fetch(`/users?id=${id}`, {
            method: "DELETE"
        }).then(response => {
            if (!response.ok) {
                throw Error(response.statusText);
            }
            location.reload();
        }).catch(error => {
            alert(error);
        })
    }
}

document.addEventListener("DOMContentLoaded", () => {
    currentUser();
});
