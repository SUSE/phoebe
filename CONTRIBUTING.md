# How to Contribute
We'd love to accept your patches and contributions to this project. There are just a few small guidelines you need to follow.

# Issues
Please, use the Issues feature for any work happening on the code.

# Git
* Each bug/feature must have its own branch.
* You create the branch locally (avoiding to fork the project).
* Don't push directly to the master branch.
* The PR must be validated, don't merge your own code.
* Squash the commits to one for each PR.
* The PR must have a reference to an issue.
* You should sign your commits.

# Recommend process
## Environment setup
* Fork this project to your individual account.
* Clone this project to your R&D environment, like _git clone git@github.com:<username>/phoebe.git_
* In your directory, configure upstream: _git remote add upstream git@github.com:SUSE/phoebe.git_
* Do **not** push to upstream: _git remote set-url --push upstream no_push_
* Configure you name for this project: _git config user.name "your name"_
* Configure your email for this project: _git config user.email "name.surname@domain.com"_

## Update commits from upstream
* Fetch commits from upstream: _git fetch upstream_
* Merge to your repo: _git merge upstream/master_, we recommend only merge upstream commits to your master branch, and merge you master branch to the branch which you want to update.

## Create pull request
* Creat a branch for your PR: _git checkout -b <proposal>_
* Develop your code.
* Ensure your code adheres to the coding standards: run _ninja clang-format_ and _ninja clang-tidy_ in the build directory.
* Ensure the code builds successfully on your machine
* Push to your repo: _git push origin <proposal>_
* In your repo, choose <proposal> branch, then create pull request.
* Fill the pull request template and click "Create".
