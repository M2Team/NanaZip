let isWindows = false;
let isEdge = false;
let cspErrorOccurred = false;

function detectEnvironment() {
    try {
        if (navigator.userAgentData) {
            isWindows = navigator.userAgentData.platform === 'Windows';
            isEdge = navigator.userAgentData.brands.some((brand) => brand.brand === 'Microsoft Edge');
            return;
        }
    } catch {
        // Eat the error
    }

    isWindows = navigator.userAgent.includes('Windows NT');
    isEdge = navigator.userAgent.includes('Edg/');
}

detectEnvironment();

if (isWindows && isEdge) {
    const iframe = document.createElement('iframe');
    iframe.id = 'microsoft-consent-iframe';
    iframe.src = "https://get.microsoft.com/iframe.html";
    iframe.style.display = 'none';
    document.body.appendChild(iframe);
}

function launchViaProtocolOnCspError(productId: string) {
    // Listen for content security policy (CSP) errors. This error can happen if the user's domain
    // has blocked iframe navigation to our whitelisted get.microsoft.com domain via their CSP.
    // If that happens, we'll fallback to launching the app via the ms-store-app:// protocol.
    const cspErrorEventName = "securitypolicyviolation";
    const cspErrorListener = (e: SecurityPolicyViolationEvent) => {
        if (e.violatedDirective === "frame-src" && e.type === cspErrorEventName) {
            cspErrorOccurred = true;
            launchFullStoreApp(productId);
        }
    }
    document.addEventListener(cspErrorEventName, cspErrorListener, { once: true }); // Once, because we'll only get the error once, even if we try to launch multiple times.

    // Remove the CSP error listener 2s after we try to launch. We don't want to listen for other CSP errors that may exist on the page.
    setTimeout(() => document.removeEventListener(cspErrorEventName, cspErrorListener), 2000);
}

function launchFullStoreApp(productId: string) {
    const query = new URLSearchParams();
    query.set('productid', productId || '');
    query.set('referrer', 'appbadge');
    location.href = "ms-windows-store://pdp/?" + query.toString();
}

const badges = document.querySelectorAll('.store-badge-link');
badges.forEach((badge) => {
    badge.addEventListener('click', (event) => {
        event.preventDefault();
        const dataset = (badge as HTMLElement).dataset;
        if (isWindows && isEdge) {
            const iframe = document.getElementById('microsoft-consent-iframe') as HTMLIFrameElement;
            if (cspErrorOccurred || !iframe || !iframe.contentWindow) {
                launchFullStoreApp(dataset.productId || '');
            } else {
                launchViaProtocolOnCspError(dataset.productId || '');

                const args = {
                    message: "launch",
                    productId: dataset.productId,
                    windowMode: "full"
                };
                iframe.contentWindow.postMessage(args, "*");
            }
        } else if (isWindows) {
            launchFullStoreApp(dataset.productId || '');
        } else {
            const query = new URLSearchParams();
            query.set('referrer', 'appbadge');
            const url = `https://apps.microsoft.com/detail/${dataset.productId}?${query.toString()}`;

            if (event instanceof MouseEvent && event.ctrlKey) {
                window.open(url, '_blank');
            } else {
                location.href = url;
            }
        }
    });
});