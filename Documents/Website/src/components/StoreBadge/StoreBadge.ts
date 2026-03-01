function changeStoreBadge(darkMode: boolean) {
    document.querySelectorAll("ms-store-badge").forEach((badge) => {
        badge.setAttribute("theme", darkMode ? "light" : "dark");
    });
}

window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', function (e) {
    changeStoreBadge(e.matches);
});