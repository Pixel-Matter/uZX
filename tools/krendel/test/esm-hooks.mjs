// @kabelsalat/web, a dependency of @strudel/core, has no "exports" and names its UMD build as "main",
// so Node loads the wrong file. Point it at the ES module build, as bundlers do through "module".
export async function resolve(specifier, context, next) {
    if (specifier === '@kabelsalat/web') {
        return next('@kabelsalat/web/dist/index.mjs', context);
    }
    return next(specifier, context);
}
