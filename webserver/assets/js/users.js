"use strict";

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
                response.text().then(data => {
                    alert(data);
                }).catch(() => {
                    throw new Error("Issue deleting user");
                })
            }
            else { location.reload() };
        }).catch(error => {
            alert(error);
        })
    }
}

function modifyUser() {
    fetch(`/user`, {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({
            "username": document.getElementById("new-username").value,
            "password": document.getElementById("new-password").value
        })
    }).then(response => {
        if (!response.ok) {
            response.text().then(data => {
                document.getElementById("modify-user-message").innerHTML = data;
            }).catch(() => {
                throw new Error("Issue modifying user");
            })
        } else {
            location.reload();
        }
    }).catch(error => {
        alert(error);
    });
}
