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

function nextPage() {
    fillCommandList(1);
}

function prevPage() {
    fillCommandList(-1);
}

function firstPage() {
    fillCommandList(-document.getElementById("current-page").value);
}

function lastPage() {
    fillCommandList(document.getElementById("last-page").value);
}

function fillCommandList(page_offset = 0) {
    let page = Number(document.getElementById("current-page").value) + page_offset;
    let filter = Number(document.getElementById("command-filter").value);
    if (page < 0) {
        page = 0;
    }
    if (filter < 0) {
        filter = 0;
    }

    const prev = document.getElementById("prev-page");
    if (page == 0) {
        prev.disabled = true;
        prev.hidden = true;
    }
    else {
        prev.disabled = false;
        prev.hidden = false;
    }

    fetch(`/control`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
            "page": page,
            "filter_type": filter
        })
    }).then(response => response.text()).then(data => {
        const json = JSON.parse(data);
        console.log(json);
        document.getElementById("current-page").textContent = `${json.page + 1} / ${json.pages}`;
        document.getElementById("current-page").value = json.page;
        document.getElementById("last-page").value = json.pages - 1;

        const next = document.getElementById("next-page");
        if (json.page == json.pages - 1) {
            next.disabled = true;
            next.hidden = true;
        }
        else {
            next.disabled = false;
            next.hidden = false;
        }

        const last = document.getElementById("last-page");
        if (json.page == json.pages - 1) {
            last.disabled = true;
            last.hidden = true;
        }
        else {
            last.disabled = false;
            last.hidden = false;
        }

        const commands = json.commands;
        console.log(commands);
        const table = document.getElementById("command-list");
        table.innerHTML = "";

        commands.forEach(command => {
            console.log(command);
            const row = document.createElement("tr");
            const id = document.createElement("td");
            const target = document.createElement("td");
            const position = document.createElement("td");
            const key_name = document.createElement("td");
            const key_id = document.createElement("td");
            const status = document.createElement("td");
            const time = document.createElement("td");
            console.log("Elements created");
            target.textContent = command.target;
            position.textContent = command.position;
            id.textContent = command.id;
            key_name.textContent = command.name;
            key_id.textContent = command.key_id;
            status.textContent = command.status;
            time.textContent = command.datetime;
            console.log("Elements set");
            row.appendChild(id);
            row.appendChild(target);
            row.appendChild(position);
            row.appendChild(key_name);
            row.appendChild(key_id);
            row.appendChild(status);
            row.appendChild(time);
            console.log("Row created");
            table.appendChild(row);
            console.log("Row appended");
        });

        deleteButtons();
    }).catch(() => {
        const row = document.createElement("tr");
        const cell = document.createElement("td");
        cell.textContent = "Error loading commands";
        row.appendChild(cell);
        document.getElementById("command-table").appendChild(row);
    })
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
    const table = document.getElementById("command-table");
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
    fillCommandList(0, 0);
});

