# How to Contribute
We'd love to accept your patches and contributions to this project. There are just a few small guidelines you need to follow.

# Issues
Please, use the Issues feature for any work happening on the code.

# Git
* Each bug/feature must have its own branch.
* Don't push directly to the main branch.
* The PR must be validated, don't merge your own code.
* Squash the commits to one for each PR.
* The PR must have a reference to an issue.
* You should sign your commits.

# Recommend process
## Environment setup
* Fork this project to your individual account.
* Clone this project to your R&D environment, like `git clone git@github.com:<username>/phoebe.git`
* In your directory, configure upstream: `git remote add upstream git@github.com:SUSE/phoebe.git`
* Do **not** push to upstream: `git remote set-url --push upstream no_push`
* Configure you name for this project: `git config user.name "your name"`
* Configure your email for this project: `git config user.email "name.surname@domain.com"`

## Update commits from upstream
* Fetch commits from upstream: `git fetch upstream`
* Merge to your repo: `git merge upstream/main`, we recommend only merge upstream commits to your main branch, and merge you main branch to the branch which you want to update.

## Create pull request
* Creat a branch for your PR: `git checkout -b <proposal>`
* Develop your code and follow the (http://llvm.org/docs/CodingStandards.html)[llvm coding standard).
* Ensure your code adheres to the coding standards: run `ninja clang-format` and `ninja clang-tidy` in the build directory.
* Ensure the code builds successfully on your machine
* Push to your repo: _git push origin <proposal>_
* In your repo, choose <proposal> branch, then create pull request.
* Fill the pull request template and click "Create".
