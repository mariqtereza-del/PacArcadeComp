# Putting the project on GitHub: a click-by-click guide for beginners

You need no experience. Take your time; nothing here can break the game, and anything you do on GitHub can be undone (see "If something goes wrong" at the end). The screens of GitHub change a little over time, so a button may be worded slightly differently; the idea stays the same.

**What you will have at the end:** a public web page (the repository) with the documentation and all the source code, plus a **Release** that lets anyone download the Windows program (without the game's ROM).

**What you need first (two things I give you):**
1. `maze-chase-github.zip`: the clean copy of the project. It has been scanned: no ROM, nothing made from the ROM.
2. `MazeChase-1.0.1-win64.zip`: the Windows program for the Release.

Do **not** upload your own `pacman.zip` anywhere. It is not in either file, and the repository's `.gitignore` file makes GitHub Desktop ignore it even if you drop it in by mistake.

---

## Part 1: Make a GitHub account (skip if you have one)
1. Open **https://github.com** in your browser.
2. Click **Sign up** (top right). Enter your email, a password and a user name. **Your user name becomes part of the web address** (for example `github.com/yourname/maze-chase`), so choose one you are happy to show.
3. Solve the little puzzle, click **Create account**, type the code GitHub emails you.
4. GitHub asks a few questions; you can click **Skip** on all of them.

## Part 2: Install GitHub Desktop (the easy way to upload)
GitHub Desktop is a free program that uploads a folder for you, with buttons instead of commands.
1. Go to **https://desktop.github.com** and click **Download for Windows**.
2. Open the downloaded installer (`GitHubDesktopSetup-x64.exe`). It installs by itself and opens.
3. Click **Sign in to GitHub.com**. Your browser opens; click **Authorize desktop** (sign in first if asked), then **Open GitHub Desktop** if the browser asks.
4. **Configure Git** screen: Name: your name or user name. Email: choose **Use my GitHub-provided noreply email** if offered (this keeps your real email address out of the public project). Click **Finish**.

## Part 3: Prepare the folder
1. Find `maze-chase-github.zip`. Right-click it, **Extract All...**, **Extract**. You now have a folder `maze-chase-github` with files such as `README.md`, `src`, `docs`, `tests` inside.
2. *Safety look:* in that folder click the search box (top right of the window) and type `pacman.zip`. Windows must find **nothing**. Also check there is no folder named `roms`. (There is not; this is only a double check.)
3. Make hidden files visible so nothing is missed: in File Explorer click **View**, then **Show**, then tick **Hidden items**. You should now see `.gitignore`, `.clang-format` and `.github_export` in the folder.

## Part 4: Create the repository and upload
1. In GitHub Desktop click **File**, then **New repository...**
2. Fill in:
   - **Name:** `maze-chase`
   - **Description:** `An arcade-accurate Windows player for a classic maze game, with modern extras. Bring your own ROM.`
   - **Local path:** leave as it is (it shows where GitHub Desktop will make the folder, for example `Documents\GitHub`).
   - **Initialize this repository with a README:** leave **unticked** (we already have one).
   - **Git ignore:** `None`. **License:** `None`.
3. Click **Create repository**. GitHub Desktop now has an empty repository folder `...\GitHub\maze-chase`.
4. Click **Show in Explorer** (or **Repository > Show in Explorer**) to open that folder.
5. In the other window open the extracted `maze-chase-github` folder, press **Ctrl + A** (select everything), **Ctrl + C** (copy). Go to the `maze-chase` folder and press **Ctrl + V** (paste). Do **not** copy the folder itself, but what is **inside** it, so that `README.md` sits directly in `maze-chase`.
6. Go back to GitHub Desktop. The left side lists hundreds of changed files (about 500+, all green plus signs). That is right.
7. At the bottom left, in the box **Summary (required)** type: `First upload`. Click the blue **Commit to main** button.
8. Click **Publish repository** (top, blue). In the window:
   - **Name** and **Description** are already filled in.
   - **Untick "Keep this code private"** so that everybody can see it. (If you want to look first, leave it ticked and make it public later: Part 6.)
   - Click **Publish repository**. Wait until the progress bar finishes.
9. Click **Repository > View on GitHub** (menu at the top). Your repository page opens in the browser.

### Alternative: upload with the web page (no program to install)
Use this only if GitHub Desktop does not work for you. The web page takes at most 100 files per drop, so you do it in several rounds.
1. Open https://github.com, click the **+** at the top right, choose **New repository**.
2. **Repository name:** `maze-chase`. Description as above. Choose **Public**. Leave "Add a README", ".gitignore" and "license" **off**. Click **Create repository**.
3. On the empty repository page click **uploading an existing file**.
4. Open the extracted `maze-chase-github` folder. Drag the folders and files into the browser window **a few at a time** (for example: first `README.md`, `LICENSE` and the other loose files together with `docs`; then `src`; then `tests`; then `tools`). Folders keep their structure. Wait for the progress bar each time.
5. At the bottom, in the box **Commit changes** type `Upload part 1`, keep **Commit directly to the main branch**, click **Commit changes**.
6. Repeat from step 3 (**Add file > Upload files**) for the next part, until every folder is in. Do not forget the three hidden files `.gitignore`, `.clang-format` and `.github_export`.

## Part 5: Write the "About" text and topics
1. On your repository page, to the right of the file list, click the **gear icon** next to **About**.
2. **Description:** `Arcade-accurate Windows player for a classic maze game (bring your own ROM). Z80 emulator, modern front end, tested.`
3. **Topics** (type each, press Enter): `emulator`, `z80`, `arcade`, `sdl2`, `cpp`, `retro-gaming`, `windows`.
4. Leave **Website** empty. Tick **Releases** (it will appear after Part 6). Click **Save changes**.

## Part 6: Publish the Windows program as a Release
1. On the repository page click **Releases** (right side) or go to `https://github.com/YOURNAME/maze-chase/releases`. Click **Create a new release** (or **Draft a new release**).
2. **Choose a tag:** type `v1.0.1`, then click **Create new tag: v1.0.1 on publish**.
3. **Release title:** `Maze Chase 1.0.1 (Windows, 64-bit)`.
4. **Description:** paste this:

   ```
   Windows 64-bit build of Maze Chase (working title).

   IMPORTANT: this download does NOT contain the game. You need your own copy of the ROM set, a file named pacman.zip.

   How to start: download MazeChase-1.0.1-win64.zip, right-click > Extract All, put your pacman.zip next to MazeChase.exe, double-click MazeChase.exe.
   If Windows shows "Windows protected your PC": More info > Run anyway (the program is new and not signed with a paid certificate).

   Full instructions: see the README on the front page of this repository.
   ```
5. Near the bottom, in the box **Attach binaries by dropping them here or selecting them**, drag in `MazeChase-1.0.1-win64.zip` (or click the box and choose it). Wait until it shows as uploaded.
6. Make sure **Set as the latest release** is ticked and **Set as a pre-release** is not. Click **Publish release**.

## Part 7: Check it as a stranger would see it (important)
1. Copy the address of your repository from the browser's address bar.
2. Open a **private window**: in Edge press **Ctrl + Shift + N**, in Chrome **Ctrl + Shift + N**, in Firefox **Ctrl + Shift + P**. Paste the address and press Enter. (In a private window you are not signed in, so you see exactly what the public sees. If it says "404", the repository is still private: see "Making a private repository public" below.)
3. Check, one by one:
   - The front page shows the **README** with the title "Maze Chase (working title)" and the **menu picture** under the first paragraph.
   - Scroll down: the other three pictures appear; the links in the contents list jump to the right places.
   - Click `BUILDING.md`, `MODDING.md` and `ARCHITECTURE.md` in the file list or in the README: each opens and reads well.
   - Click **Releases** (right side): there is **Maze Chase 1.0.1** with the file `MazeChase-1.0.1-win64.zip` under **Assets**. Click it: the download starts. (Then you may delete the download.)
   - In the file list there is **no** `roms` folder and no file called `pacman.zip`. In the search box at the top of the repository (or press `t` on the repository page) type `pacman.zip`: there should be no such file.
4. If everything is right, you are done.

## Making a private repository public (or public private)
Repository page > **Settings** (tab at the top) > scroll to the very bottom, **Danger Zone** > **Change repository visibility** > follow the questions. (Making it public is what lets everyone see it.)

## If something goes wrong
- **You uploaded something you did not want to:** do not panic. Repository page > **Settings** > **Danger Zone** > **Delete this repository**, type the name to confirm, and start again from Part 4. Nothing on your computer is affected.
- **GitHub Desktop shows an error:** close it, open it again, and click **Publish repository** again. If it still fails, use the web-page alternative.
- **A picture does not show in the README:** check that the folder `docs/images` was uploaded (four `.png` files). Upload it again if it is missing.
- **You see your real email address in a commit:** in GitHub, Settings > Emails, tick **Keep my email addresses private** and **Block command line pushes that expose my email**. Future uploads then use the private address.
- **Someone asks for the ROM in an issue:** do not provide it or links to it. See CONTRIBUTING.md.

## Later: updating the repository after changes
Changes come to you as a new `maze-chase-github.zip` (made with `tools/export_github.sh`, which scans for ROM bytes before it finishes). Extract it, copy its contents over the `maze-chase` folder (replace files), open GitHub Desktop, write a summary such as `Update documentation`, **Commit to main**, then **Push origin**. For a new program version, repeat Part 6 with a new tag (for example `v1.0.2`).

## How the clean copy is made (for the record)
`sh tools/export_github.sh OUTPUT_FOLDER` (on Linux, in the project folder; needs your ROM only to scan for it) copies the project without the ROM, build output, the release zip, the disassembly, the user's private notes and the screenshots of the game's graphics; then it searches every exported file for runs of 16 bytes taken from any ROM file (as raw bytes, hexadecimal text and number tables) and refuses to finish if it finds one. What is left out and why: `docs/evidence/S37_github.txt`.
