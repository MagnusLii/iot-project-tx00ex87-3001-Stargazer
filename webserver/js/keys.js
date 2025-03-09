"use strict";

function generateKey() {
    const name = document.getElementById("name-key").value;
    if (name == "") {
        alert("Please enter a name");
        return;
    }

    fetch(`/control/keys`, {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({ "name": name })
    }).then(response => response.text()).then(data => {
        document.getElementById("ro-key").value = data;
    }).catch(() => {
        alert("Issue generating key");
    });
}

function deleteKey(id) {
    if (confirm("Are you sure you want to delete this key?") == true) {
        fetch(`/control/keys?id=${id}`, {
            method: "DELETE"
        }).then(() => {
            location.reload();
        }).catch(() => {
            alert("Issue deleting key");
        });
    }
}

