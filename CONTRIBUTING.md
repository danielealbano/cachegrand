# Contributing to cachegrand

When contributing to the development of cachegrand, please first discuss the change
you wish to make via issue, email, or any other method with the maintainers before
making a change.

Please note we have a [Code of Conduct](CODE_OF_CONDUCT.md), please follow it in all
your interactions with the project.

# Table of contents
1. [Creating an issue](#issues-issues-and-more-issues)
2. [Template](#issue-template)
4. [Pull requests](#pull-request-process)

# Issues, issues and more issues!

There are many ways you can contribute to cachegrand but all of them involve creating issues
in [cachegrand issue tracker](https://github.com/danielealbano/cachegrand/issues). This is the
entry point for your contribution.

To create an effective and high quality ticket, try to put the following information on your
ticket:

- A detailed description of the issue or feature request
  - For issues, please add the necessary steps to reproduce the issue.
    - Config file
    - Hardware and Operating System
        - Specify if it's a VM running on the cloud, a VM running on on-premises hardware or if it's hardware on premises
        - Specify if it's docker, kubernetes, openshift, etc.
  - For feature requests, add a detailed description of your proposal.
- A checklist of Development tasks
- A checklist of QA tasks

## Issue template
```
[Title of the issue or feature request]

... detailed description of the issue ...

## Steps to reproduce:

1. Set the config file parameters XYZ to ZYX
2. Start cachegrand
3. Do the following operations ...

## Config file

... config file ...

## Hardware and Operating System

Virtual Machine on the Cloud, Azure, Standard_E8_v5
Ubuntu 22.04

## Development Tasks

List of development tasks suggested below

* [ ]  development tasks

## QA Tasks

List of QA tasks suggested below

* [ ]  qa (quality assurance) tasks
```

# Pull Request Process

1. Ensure your code compiles. Run `make` before creating the pull request.
2. If you're adding new API or commands, they must be properly documented.
3. The commit message is formatted as follows:

```
   A paragraph explaining the problem and its context.

   Another one explaining how you solved that.

   <link to the issue on github>
```

4. You may merge the pull request in once you have the sign-off of the maintainers, or if you
   do not have permission to do that, you may request the second reviewer to merge it for you.
---
