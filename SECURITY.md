# Credentials and public releases

Turnforge is an offline game. Playing it requires no account, API key, deployment token, or environment file.

Local environment files, private keys, authentication files, and provider configuration are excluded by `.gitignore`. GitHub and hosting credentials belong in the local credential manager or the hosting provider's secret settings. Include only public URLs in documentation and portfolio links.

Before making a release public, scan the complete reachable Git history, the staged changes, and the extracted Windows package with a secret scanner. The publication workflow uses Gitleaks with full redaction; scanner output and working reports stay under the ignored `build/security/` directory. Check that the package contains only the intended executable, artwork, documentation, and third-party notices.

The game executable and public browser assets do not need hosting credentials. Repeat the scan after future changes that introduce external services or build-time configuration.
