# PacArcadeComp on GitHub: a click-by-click guide for beginners

**Which part do you need?**
- You already published the project before (the repository was called `maze-chase`) and want to bring it to version 1.1.0 with the new name PacArcadeComp: do **"Update guide (version 1.1.0 and the new name)"** right below, then **Part 7** (check).
- You have never published it: skip the update guide and start at Part 1.

---

# Update guide (version 1.1.0 and the new name)

Nothing here can break the game, and nothing touches your own `pacman.zip`.

**You need two files from me:** `PacArcadeComp-github.zip` (the clean copy of the project, scanned: no ROM) and `PacArcadeComp-1.1.0-win64.zip` (the Windows program for the Release).

## U1: Rename the repository on GitHub
1. Open **https://github.com** in your browser and sign in. Open your repository (click your picture at the top right, **Your repositories**, then **maze-chase**).
2. Click the **Settings** tab (top row of the repository page, far right; if you cannot see it, click the three dots **...** first).
3. On the page that opens, the first box is **Repository name**. Click in it, delete `maze-chase`, type `PacArcadeComp`, then click the **Rename** button next to it. The page reloads and the address now ends in `/PacArcadeComp`. (Old links to the old name keep working; GitHub forwards them.)
4. Scroll back to the top of the page and click **Code** (first tab) to return to the repository page.

## U2: Tell GitHub Desktop about the new name
1. Open **GitHub Desktop**. Make sure the repository shown at the top left is your project (click that box to switch if needed).
2. Click **Repository** in the menu at the top, then **Repository settings...**
3. Click **Remote** on the left. In the box **Primary remote repository (origin)** replace `maze-chase` at the end of the address with `PacArcadeComp` (for example `https://github.com/YOURNAME/PacArcadeComp.git`). Click **Save**.
4. (Optional) The folder on your computer may still be called `maze-chase`. That is fine; leave it. Everything below uses that folder, whatever it is called.

## U3: Replace the files with the new ones
1. Find `PacArcadeComp-github.zip` (where you saved my file). Right-click it, **Extract All...**, **Extract**. You get a folder `PacArcadeComp-github`.
2. Safety look: in that folder click the search box (top right) and type `pacman.zip`. Windows must find nothing.
3. In GitHub Desktop click **Repository > Show in Explorer**. The folder of your repository opens.
4. Do **not** delete anything in that folder (it contains an invisible folder `.git` that must stay).
5. Open the folder `PacArcadeComp-github`, press **Ctrl + A**, **Ctrl + C**. Go back to the repository folder and press **Ctrl + V**. When Windows asks about files with the same names, click **Replace the files in the destination**. The files must sit directly inside the repository folder (`README.md` is right there, not inside another folder). Tick **Hidden items** (View > Show) and make sure `.gitignore`, `.clang-format` and `.github_export` were copied too.

## U4: Commit and upload (push)
1. Back in GitHub Desktop the left side lists the changed files (many). That is right.
2. In the box **Summary (required)** at the bottom left type: `Version 1.1.0: rules, mods, new name PacArcadeComp`.
3. Click the blue **Commit to main** button.
4. Click **Push origin** (blue button at the top). Wait until it says **Last fetched just now**.
5. Click **Repository > View on GitHub** to open the page.

## U5: Update the About text
1. On the repository page click the **gear icon** next to **About** (right side).
2. **Description:** `Arcade-accurate Windows player for a classic maze game (bring your own ROM). Z80 emulator, modern front end, readable rules, mods.`
3. Add the topic `modding` (type it, press Enter). Click **Save changes**.

## U6: Publish the release v1.1.0
1. On the repository page click **Releases** (right side), then **Draft a new release** (or **Create a new release**).
2. **Choose a tag:** type `v1.1.0`, then click **Create new tag: v1.1.0 on publish**.
3. **Release title:** `PacArcadeComp 1.1.0 (Windows, 64-bit)`.
4. **Description:** paste the text below.

   ```
   PacArcadeComp 1.1.0 (Windows, 64-bit). The project was formerly called Maze Chase.

   New in 1.1.0:
   - Readable game rules (Esc > GAME OPTIONS > RULES > READABLE), proved identical to the arcade routines frame by frame.
   - Modding: a rules.cfg file changes speeds, times, points and more. Three example mods are in the mods folder; MODDING.txt explains it in a few lines, MODDING.md in the repository fully. Modded games say MODDED in the window title and never count for records.
   - The zip now contains the mods folder and MODDING.txt.

   IMPORTANT: this download does NOT contain the game. You need your own copy of the ROM set, a file named pacman.zip.

   How to start: download PacArcadeComp-1.1.0-win64.zip, right-click > Extract All, put your pacman.zip next to PacArcadeComp.exe, double-click PacArcadeComp.exe.
   If Windows shows "Windows protected your PC": More info > Run anyway (the program is not signed with a paid certificate).
   Your settings, records and high score are kept (they live in %APPDATA%\PacmanPort).
   ```
5. In the box **Attach binaries by dropping them here or selecting them**, drag in `PacArcadeComp-1.1.0-win64.zip` and wait until it shows as uploaded.
6. Make sure **Set as the latest release** is ticked and **Set as a pre-release** is not. Click **Publish release**.
7. The old release v1.0.1 may stay (it is the old program under the old name). If you prefer, open it (Releases > the old release > the pencil icon) and tick **Set as a pre-release**, or delete it.

Now go to **Part 7** to check the page as a stranger sees it.

---

# First-time guide (only if you have never published it)

You need no experience. Take your time; nothing here can break the game, and anything you do on GitHub can be undone (see "If something goes wrong" at the end). The screens of GitHub change a little over time, so a button may be worded slightly differently; the idea stays the same.

**What you will have at the end:** a public web page (the repository) with the documentation and all the source code, plus a **Release** that lets anyone download the Windows program (without the game's ROM).

**What you need first (two things I give you):**
1. `PacArcadeComp-github.zip`: the clean copy of the project. It has been scanned: no ROM, nothing made from the ROM.
2. `PacArcadeComp-1.1.0-win64.zip`: the Windows program for the Release.

Do **not** upload your own `pacman.zip` anywhere. It is not in either file, and the repository's `.gitignore` file makes GitHub Desktop ignore it even if you drop it in by mistake.

---

## Part 1: Make a GitHub account (skip if you have one)
1. Open **https://github.com** in your browser.
2. Click **Sign up** (top right). Enter your email, a password and a user name. **Your user name becomes part of the web address** (for example `github.com/yourname/PacArcadeComp`), so choose one you are happy to show.
3. Solve the little puzzle, click **Create account**, type the code GitHub emails you.
4. GitHub asks a few questions; you can click **Skip** on all of them.

## Part 2: Install GitHub Desktop (the easy way to upload)
GitHub Desktop is a free program that uploads a folder for you, with buttons instead of commands.
1. Go to **https://desktop.github.com** and click **Download for Windows**.
2. Open the downloaded installer (`GitHubDesktopSetup-x64.exe`). It installs by itself and opens.
3. Click **Sign in to GitHub.com**. Your browser opens; click **Authorize desktop** (sign in first if asked), then **Open GitHub Desktop** if the browser asks.
4. **Configure Git** screen: Name: your name or user name. Email: choose **Use my GitHub-provided noreply email** if offered (this keeps your real email address out of the public project). Click **Finish**.

## Part 3: Prepare the folder
1. Find `PacArcadeComp-github.zip`. Right-click it, **Extract All...**, **Extract**. You now have a folder `PacArcadeComp-github` with files such as `README.md`, `src`, `docs`, `tests` inside.
2. *Safety look:* in that folder click the search box (top right of the window) and type `pacman.zip`. Windows must find **nothing**. Also check there is no folder named `roms`. (There is not; this is only a double check.)
3. Make hidden files visible so nothing is missed: in File Explorer click **View**, then **Show**, then tick **Hidden items**. You should now see `.gitignore`, `.clang-format` and `.github_export` in the folder.

## Part 4: Create the repository and upload
1. In GitHub Desktop click **File**, then **New repository...**
2. Fill in:
   - **Name:** `PacArcadeComp`
   - **Description:** `An arcade-accurate Windows player for a classic maze game, with modern extras. Bring your own ROM.`
   - **Local path:** leave as it is (it shows where GitHub Desktop will make the folder, for example `Documents\GitHub`).
   - **Initialize this repository with a README:** leave **unticked** (we already have one).
   - **Git ignore:** `None`. **License:** `None`.
3. Click **Create repository**. GitHub Desktop now has an empty repository folder `...\GitHub\PacArcadeComp`.
4. Click **Show in Explorer** (or **Repository > Show in Explorer**) to open that folder.
5. In the other window open the extracted `PacArcadeComp-github` folder, press **Ctrl + A** (select everything), **Ctrl + C** (copy). Go to the `PacArcadeComp` folder and press **Ctrl + V** (paste). Do **not** copy the folder itself, but what is **inside** it, so that `README.md` sits directly in `PacArcadeComp`.
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
2. **Repository name:** `PacArcadeComp`. Description as above. Choose **Public**. Leave "Add a README", ".gitignore" and "license" **off**. Click **Create repository**.
3. On the empty repository page click **uploading an existing file**.
4. Open the extracted `PacArcadeComp-github` folder. Drag the folders and files into the browser window **a few at a time** (for example: first `README.md`, `LICENSE` and the other loose files together with `docs`; then `src`; then `tests`; then `tools`). Folders keep their structure. Wait for the progress bar each time.
5. At the bottom, in the box **Commit changes** type `Upload part 1`, keep **Commit directly to the main branch**, click **Commit changes**.
6. Repeat from step 3 (**Add file > Upload files**) for the next part, until every folder is in. Do not forget the three hidden files `.gitignore`, `.clang-format` and `.github_export`.

## Part 5: Write the "About" text and topics
1. On your repository page, to the right of the file list, click the **gear icon** next to **About**.
2. **Description:** `Arcade-accurate Windows player for a classic maze game (bring your own ROM). Z80 emulator, modern front end, tested.`
3. **Topics** (type each, press Enter): `emulator`, `z80`, `arcade`, `sdl2`, `cpp`, `retro-gaming`, `windows`.
4. Leave **Website** empty. Tick **Releases** (it will appear after Part 6). Click **Save changes**.

## Part 6: Publish the Windows program as a Release
1. On the repository page click **Releases** (right side) or go to `https://github.com/YOURNAME/PacArcadeComp/releases`. Click **Create a new release** (or **Draft a new release**).
2. **Choose a tag:** type `v1.1.0`, then click **Create new tag: v1.1.0 on publish**.
3. **Release title:** `PacArcadeComp 1.1.0 (Windows, 64-bit)`.
4. **Description:** paste this:

   ```
   Windows 64-bit build of PacArcadeComp.

   IMPORTANT: this download does NOT contain the game. You need your own copy of the ROM set, a file named pacman.zip.

   How to start: download PacArcadeComp-1.1.0-win64.zip, right-click > Extract All, put your pacman.zip next to PacArcadeComp.exe, double-click PacArcadeComp.exe.
   If Windows shows "Windows protected your PC": More info > Run anyway (the program is new and not signed with a paid certificate).

   Full instructions: see the README on the front page of this repository.
   ```
5. Near the bottom, in the box **Attach binaries by dropping them here or selecting them**, drag in `PacArcadeComp-1.1.0-win64.zip` (or click the box and choose it). Wait until it shows as uploaded.
6. Make sure **Set as the latest release** is ticked and **Set as a pre-release** is not. Click **Publish release**.

## Part 7: Check it as a stranger would see it (important)
1. Copy the address of your repository from the browser's address bar.
2. Open a **private window**: in Edge press **Ctrl + Shift + N**, in Chrome **Ctrl + Shift + N**, in Firefox **Ctrl + Shift + P**. Paste the address and press Enter. (In a private window you are not signed in, so you see exactly what the public sees. If it says "404", the repository is still private: see "Making a private repository public" below.)
3. Check, one by one:
   - The front page shows the **README** with the title "PacArcadeComp" and the **menu picture** under the first paragraph.
   - Scroll down: the other three pictures appear; the links in the contents list jump to the right places.
   - Click `BUILDING.md`, `MODDING.md` and `ARCHITECTURE.md` in the file list or in the README: each opens and reads well.
   - Click **Releases** (right side): there is **PacArcadeComp 1.1.0** with the file `PacArcadeComp-1.1.0-win64.zip` under **Assets**. Click it: the download starts. (Then you may delete the download.)
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
Changes come to you as a new `PacArcadeComp-github.zip` (made with `tools/export_github.sh`, which scans for ROM bytes before it finishes). Extract it, copy its contents over the `PacArcadeComp` folder (replace files), open GitHub Desktop, write a summary such as `Update documentation`, **Commit to main**, then **Push origin**. For a new program version, repeat Part 6 with a new tag (for example `v1.1.1`).

## How the clean copy is made (for the record)
`sh tools/export_github.sh OUTPUT_FOLDER` (on Linux, in the project folder; needs your ROM only to scan for it) copies the project without the ROM, build output, the release zip, the disassembly, the user's private notes and the screenshots of the game's graphics; then it searches every exported file for runs of 16 bytes taken from any ROM file (as raw bytes, hexadecimal text and number tables) and refuses to finish if it finds one. What is left out and why: `docs/evidence/S37_github.txt`.
