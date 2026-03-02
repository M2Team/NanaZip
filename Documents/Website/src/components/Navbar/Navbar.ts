document.querySelector('.menu-button')?.addEventListener('click', () => {
    const nav = document.querySelector('nav');
    if (nav) {
        nav.classList.toggle('active');
    }
});