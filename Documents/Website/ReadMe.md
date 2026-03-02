# NanaZip Website

This folder contains the source code of the NanaZip website,
built using [Astro](https://astro.build).

## Images

When taking screenshots for the website, please note the following guidelines:
- Use the default Bloom wallpapers for Windows 11.
- Do not use Snipping Tool (or the built-in `Windows` + `Shift` + `S` shortcut),
  as it adds ugly borders around the screenshot. Use
  [ShareX](https://getsharex.com/) (with transparency on and shadows off), or
  an equivalent tool.
  - To turn on transparency mode in ShareX:
    1. Open ShareX and go to "Task settings".
    2. On the left sidebar, click on "Capture".
    3. Turn on "Capture window with transparency" and
       turn off "Capture window with shadow".
- Compress the PNG files using [pngquant](https://pngquant.org/),
  with the following command:
  ```
  pngquant --force --ext .png --quality 85-85 <input-file.png>
  ```

When adding images to the website, place them in `src/assets/images`,
and reference them like this:
```html
---
import exampleImage from '[path to picture]';
---

<Picture
  src={exampleImage}
  formats={["avif", "webp"]}
  alt="[alt text]"
  loading="[loading mode]" />
```
Where `[loading mode]` can be either `eager` or `lazy`.
Use `eager` for images that are above the fold (i.e. visible without scrolling),
and `lazy` for images that are below the fold.

If the image has both a light and a dark version, 
you can use the `ThemedLocalPicture` component instead, 
like this:
```html
---
import exampleImageLight from '[path to light version of picture]';
import exampleImageDark from '[path to dark version of picture]';
---

<ThemedLocalPicture
  lightSrc={exampleImageLight}
  darkSrc={exampleImageDark}
  alt="[alt text]"
  loading="[loading mode]" />
```