# Boot image

`wordmark.png` is the latest user-supplied 320 × 109 black-and-white crop.
Regenerate the 4,360-byte packed 1-bit asset with
`python tools/generate-boot-image/generate.py` (Pillow required), then format
`src/platform/hal/generated/BootImage.h` with clang-format. Pixels are thresholded
at 230, biased toward black. The title region (rows 4–59) retains round cores
of radius 3.5 px on its original 8 px dot grid, removing white bridges in the
binary source. Attribution pixels are not reshaped. No dithering or grayscale
anti-aliasing is used.

Boot centers the crop at (80, 345) on the 480 × 800 portrait screen. The HAL
composes native framebuffer rows directly, with black background and white text,
then submits the normal full refresh immediately after display initialization
and framebuffer validation, before LED, input, and RTC initialization. No Page, font rendering, grayscale planes,
or separate clearing refresh is involved. The existing refresh completion gate
protects the buffer before Home renders; no artificial display delay is added.

The panel supports 2-bit grayscale; using a 1-bit asset and refresh path here is
an explicit design choice for this boot image. Physical boot visibility and
refresh duration require device verification.
