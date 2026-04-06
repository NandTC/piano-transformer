/**
 * notarize.js — afterSign hook for electron-builder
 * Notarizes the Mac .app using Apple ID + app-specific password.
 * Only runs when APPLE_ID env var is set (i.e. in CI, not local dev).
 */
const { notarize } = require("@electron/notarize");

exports.default = async function notarizing(context) {
  if (context.electronPlatformName !== "darwin") return;
  if (!process.env.APPLE_ID) {
    console.log("[notarize] Skipping — APPLE_ID not set");
    return;
  }

  const appName     = context.packager.appInfo.productFilename;
  const appOutDir   = context.appOutDir;
  const appBundleId = context.packager.appInfo.id;
  const appPath     = `${appOutDir}/${appName}.app`;

  console.log(`[notarize] Notarizing ${appPath} (${appBundleId})…`);

  await notarize({
    appBundleId,
    appPath,
    appleId:          process.env.APPLE_ID,
    appleIdPassword:  process.env.APPLE_APP_SPECIFIC_PASSWORD,
    teamId:           process.env.APPLE_TEAM_ID,
  });

  console.log("[notarize] Done.");
};
