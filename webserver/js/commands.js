"use strict";

function queuePicture() {
    const data = JSON.stringify({
        "target": Number(document.getElementById("targets").value),
        "position": Number(document.getElementById("positions").value),
        "associated_key_id": Number(document.getElementById("selected_key").value)
    });
    //console.log(data);

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
    fillCommandList(1, true);
}

function lastPage() {
    console.log(Number(document.getElementById("total-pages").dataset.tpage));
    fillCommandList(Number(document.getElementById("total-pages").dataset.tpage), true);
}

function fillCommandList(page_n = 0, exact = false) {
    let page;
    if (exact) {
        page = page_n - 1;
    } else {
        page = Number(document.getElementById("current-page").dataset.cpage) + page_n;
    }
    let filter = Number(document.getElementById("command-filter").value);
    if (page < 0) {
        page = 0;
    }
    if (filter < 0) {
        filter = 0;
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
        document.getElementById("current-page").textContent = `${json.page + 1}`;
        document.getElementById("current-page").dataset.cpage = json.page;
        document.getElementById("total-pages").textContent = `${json.pages}`;
        document.getElementById("total-pages").dataset.tpage = json.pages;

        const first = document.getElementById("first-page");
        const prev = document.getElementById("prev-page");
        if (page == 0) {
            prev.disabled = true;
            first.disabled = true;
        }
        else {
            prev.disabled = false;
            first.disabled = false;
        }

        const next = document.getElementById("next-page");
        const last = document.getElementById("last-page");
        if (json.page == json.pages - 1) {
            next.disabled = true;
            last.disabled = true;
        }
        else {
            next.disabled = false;
            last.disabled = false;
        }

        const commands = json.commands;
        const table = document.getElementById("command-list");
        table.innerHTML = "";

        commands.forEach(command => {
            const row = document.createElement("tr");
            const id = document.createElement("td");
            const target = document.createElement("td");
            const position = document.createElement("td");
            const key_name = document.createElement("td");
            const key_id = document.createElement("td");
            const status = document.createElement("td");
            const time = document.createElement("td");
            const cancel = document.createElement("td");
            target.textContent = command.target;
            position.textContent = command.position;
            id.textContent = command.id;
            key_name.textContent = command.name;
            key_id.textContent = command.key_id;
            status.textContent = command.status;
            status.id = "status-" + command.id;
            time.textContent = command.datetime;
            cancel.className = "command-cancel";
            if (command.status == 0) {
                const deleteButton = document.createElement("button");
                deleteButton.textContent = "Cancel";
                deleteButton.id = "del-" + command.id;
                deleteButton.onclick = () => deleteCommand(command.id);
                cancel.appendChild(deleteButton);
            }
            row.appendChild(id);
            row.appendChild(target);
            row.appendChild(position);
            row.appendChild(key_name);
            row.appendChild(key_id);
            row.appendChild(status);
            row.appendChild(time);
            row.appendChild(cancel);
            table.appendChild(row);
        });
    }).catch(() => {
        const row = document.createElement("tr");
        const cell = document.createElement("td");
        cell.textContent = "Error loading commands";
        row.appendChild(cell);
        table.appendChild(row);
    })
}

function deleteCommand(id) {
    if (confirm("Are you sure you want to delete this command?") == true) {
        fetch(`/control/command?id=${id}`, {
            method: "DELETE"
        }).then(response => response.ok).then(() => {
            document.getElementById("status-" + id).textContent = "-6";
            document.getElementById("del-" + id).outerHTML = "";
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
        console.log(rows[i]);
        const row = rows[i];
        const cells = row.getElementsByTagName("td");
        const status = cells[5].textContent;
        if (status == "0") {
            const cell = document.createElement("td");
            cell.className = "command-cancel-button";
            const deleteButton = document.createElement("button");
            deleteButton.textContent = "Cancel C";
            deleteButton.setAttribute("id", "del-" + cells[0].textContent);
            console.log(cells[0].textContent);
            console.log(deleteButton.id);
            deleteButton.onclick = () => deleteCommand(cells[0].textContent);
            cell.appendChild(deleteButton);
            row.appendChild(cell);
        }
    }
}

document.addEventListener("DOMContentLoaded", () => {
    fillCommandList(0);
});

