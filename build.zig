const Builder = @import("std").build.Builder;

pub fn build(b: *Builder) void {
    const target = b.standardTargetOptions(.{});
    const mode = b.standardReleaseOptions();

    const exe = b.addExecutable("input-display", "src/main.zig");
    exe.setTarget(target);
    exe.setBuildMode(mode);
    
    exe.linkLibC();
    exe.linkSystemLibrary("sdl2");
    exe.linkSystemLibrary("xcb");

    exe.install();

    const run_cmd = exe.run();
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run input-display");
    run_step.dependOn(&run_cmd.step);
}
