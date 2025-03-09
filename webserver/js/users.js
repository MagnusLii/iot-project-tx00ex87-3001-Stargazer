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
        document.getElementById("feedback-user-message").innerHTML = data;
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
    let user = document.getElementById("new-username").value;
    let pass = document.getElementById("new-password").value;
    let json_data = "{}";
    if (user == "" && pass == "") {
        document.getElementById("feedback-user-message").innerHTML = "Please enter a username and/or password";
        return;
    } else if (user == "") {
        json_data = JSON.stringify({
            "password": pass
        })
    } else if (pass == "") {
        json_data = JSON.stringify({
            "username": user
        })
    } else {
        json_data = JSON.stringify({
            "username": user,
            "password": pass
        })
    }

    fetch(`/user`, {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: json_data
    }).then(response => {
        if (!response.ok) {
            response.text().then(data => {
                document.getElementById("feedback-user-message").innerHTML = data;
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
