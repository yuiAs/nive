# Project Guidelines

## Coding Style

Follows the Google C++ Style Guide (https://google.github.io/styleguide/cppguide.html) with the following modifications for naming conversions.

### Naming Conventions
- Variable Names (including arguments): snake\_case (e.g. `file\_path`)
- Local Variables: snake\_case preferred, camelCase acceptable. Must be consistent within function/file.
- Function Names: camelCase
- Type Names (class, struct, enum, enum class, typedef, using): PascalCase
- Concept Name: PascalCase
- Class Data Members: snake\_case with trailing underscore (e.g. `file\_path_`)
- Struct Data Members: snake\_case (same as reqular variables)
- Constant Names (constexpr, etc.): 'k' prefix + PascalCase (e.g. `kFilePath`)
- Enumerator Names: PascalCase
- Macros (discouraged): ALL\_CAPS\_WITH\_UNDERSCORES

**Notes:**
- Exceptions are allowed when aligning with STL or external library conventions (e.g., `begin()`, `end()`, `value\_type`)
- Clarity takes precedence over strict style guide compliance
- The `.clang-format` file handles code formatting; naming conventions are enforced through code review

### Comment Style
- Comments within source code should be written in English by default
- Use clear, concise English for inline comments
- Use either the `//` or `/* */` syntax, as long as you are consistent
  - While either syntax is acceptable, `//` is much more common
  - Be consistent with how you comment and what style you use where
- Keep comments up-to-date with code changes

## Commit Strategy
- Keep commits focused and atomic

### Commit Conventions
Write clear commit messages using Conventional Commits (https://www.conventionalcommits.org/en/v1.0.0/) style.

- The subject MUST BE written in English whenever possible
- For everything else, use clear language, primarily English
- If you ask a user for a JIRA ticket or GitHub Issue number and receive a meaningful response, include `Refs: <Ticket>` in the footer (e.g., if no string is returned, there is no need to include it)

## Branch Strategy
- main: For Production. DO NOT PUSH DIRECTORY.
- feature/<short-description>: Feature Developments
- fix/<short-description>: Bug Fixes
- bugfix/<short-description>: Bug Fixes (same as fix)
- release/<version>: Release Candidates. MUST BE REQURIED PR.

Create these branches from the develop branch unless otherwise instructed.  
For example, fix branches for specific feature branches do not need to be create from develop.

## PR (Pull Request) Strategy
- Write subjects using Conventional Commits (https://www.conventionalcommits.org/en/v1.0.0/) style.
- All descriptions should be written in English by default
