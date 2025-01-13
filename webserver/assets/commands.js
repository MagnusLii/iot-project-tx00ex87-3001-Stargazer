"use strict";

function queuePicture() {
    const data = JSON.stringify({
        "target": Number(document.getElementById("targets").value),
        "position": Number(document.getElementById("positions").value),
        "associated_key_id": Number(document.getElementById("selected_key").value)
    });
    console.log(data);

    fetch(`/control/command`, {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: data
    }).then(() => {
        location.reload();
    }).catch(() => {
        alert("Issue with queuing request");
    });
}

function deleteCommand(id) {
    if (confirm("Are you sure you want to delete this command?") == true) {
        fetch(`/control/command?id=${id}`, {
            method: "DELETE"
        }).then(() => {
            location.reload();
        }).catch(() => {
            alert("Issue deleting command");
        });
    }
}

// Traverse through the table and add delete buttons to rows with status 0
function deleteButtons() {
    const table = document.getElementById("commands");
    const rows = table.getElementsByTagName("tr");
    for (let i = 1; i < rows.length; i++) {
        const row = rows[i];
        const cells = row.getElementsByTagName("td");
        const status = cells[5].textContent;
        if (status == "0") {
            const cell = document.createElement("td");
            const deleteButton = document.createElement("button");
            deleteButton.textContent = "Delete";
            deleteButton.onclick = () => deleteCommand(cells[0].textContent);
            cell.appendChild(deleteButton);
            row.appendChild(cell);
        }
    }
}

document.addEventListener("DOMContentLoaded", () => {
    deleteButtons();
});

