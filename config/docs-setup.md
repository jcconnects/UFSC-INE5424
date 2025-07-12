# 📚 GitHub Pages Documentation Setup Guide

This guide explains how to set up automated documentation generation and hosting using Doxygen and GitHub Pages.

## 🎯 What This Setup Provides

1. **Automated Documentation Generation**: Every push to `main` triggers documentation rebuilding
2. **GitHub Pages Hosting**: Professional documentation available at `https://USERNAME.github.io/REPOSITORY/`
3. **GitHub Integration**: Footer links connect documentation back to source code
4. **Professional Appearance**: Clean, navigable API documentation with search functionality

## 🚀 Setup Steps

### 1. Enable GitHub Pages in Repository Settings

1. Go to your GitHub repository
2. Navigate to **Settings** → **Pages**
3. Under **Source**, select **"GitHub Actions"**
4. Click **Save**

### 2. Configure Repository Permissions

The GitHub Actions workflow requires specific permissions:

1. Go to **Settings** → **Actions** → **General**
2. Under **Workflow permissions**, select **"Read and write permissions"**
3. Check **"Allow GitHub Actions to create and approve pull requests"**
4. Click **Save**

### 3. Trigger the First Build

The documentation will automatically build when you:
- Push changes to the `main` branch
- Create a pull request against `main`
- Manually trigger via **Actions** → **Generate and Deploy Documentation** → **Run workflow**

### 4. Access Your Documentation

Once the workflow completes (typically 2-3 minutes), your documentation will be available at:

```
https://YOUR_USERNAME.github.io/YOUR_REPOSITORY_NAME/
```

## 🔧 Technical Details

### GitHub Actions Workflow (`.github/workflows/docs.yml`)

The automated workflow:

1. **Installs Dependencies**: Doxygen and Graphviz on Ubuntu
2. **Generates Documentation**: Runs `doxygen` using your `Doxyfile` configuration
3. **Deploys to GitHub Pages**: Uses the official GitHub Pages action

### Configuration Files

- **`Doxyfile`**: Main Doxygen configuration
- **`doc/assets/footer.html`**: Custom footer with GitHub links
- **`doc/.nojekyll`**: Tells GitHub Pages to serve files as-is (not process with Jekyll)

### Key Features Enabled

- **Source Code Integration**: Links from documentation to GitHub source
- **Issue Tracking**: Direct links to GitHub Issues from docs
- **Professional Styling**: Clean, modern documentation appearance
- **Search Functionality**: Full-text search within documentation
- **Mobile Responsive**: Works well on all device sizes

## 🎨 Customization Options

### Modify Footer Links

Edit `doc/assets/footer.html` to change:
- Repository URL
- Issue tracker link
- Additional navigation links

### Update Doxygen Settings

Key settings in `Doxyfile`:
- `PROJECT_NAME`: Your project title
- `PROJECT_BRIEF`: Short description
- `PROJECT_NUMBER`: Version number
- `INPUT`: Source directories to document

### Add Custom CSS

You can add custom styling by:
1. Creating a CSS file (e.g., `doc/custom.css`)
2. Setting `HTML_EXTRA_STYLESHEET = doc/custom.css` in Doxyfile

## 🔍 Local Testing

Before pushing changes, test documentation locally:

```bash
# Generate documentation locally
make docs

# Open in browser to preview
make docs-open

# Clean when done
make clean-docs
```

## 🎯 Benefits for Open Source Projects

1. **Professional Appearance**: Makes your project look polished and maintainable
2. **Easy Contribution**: New contributors can quickly understand the API
3. **No Manual Work**: Documentation stays current automatically
4. **SEO Benefits**: GitHub Pages are indexed by search engines
5. **Free Hosting**: No cost for public repositories

## 🐛 Troubleshooting

### Documentation Not Updating
- Check the **Actions** tab for workflow failures
- Ensure GitHub Pages source is set to "GitHub Actions"
- Verify repository permissions allow Actions to write

### 404 Errors
- Confirm `.nojekyll` file exists in output
- Check that `HTML_EXTRA_FILES = doc/.nojekyll` is set in Doxyfile

### Build Failures
- Review workflow logs in **Actions** tab
- Ensure all source files compile without errors
- Check Doxygen configuration syntax

## 🌟 Example Projects Using This Setup

This pattern is used by many successful open source projects:
- **LLVM**: https://llvm.github.io/llvm-project/
- **Boost**: https://www.boost.org/doc/
- **OpenCV**: https://docs.opencv.org/

Your project now has the same professional documentation infrastructure!

---

**🎉 Once configured, your documentation will automatically stay in sync with your code changes, providing a professional API reference for users and contributors.** 