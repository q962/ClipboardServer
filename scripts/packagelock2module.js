#!node

import { fileURLToPath } from 'url';
import { dirname } from 'path';
import { createWriteStream } from "node:fs";
import { Buffer } from "node:buffer";

process.chdir(dirname(fileURLToPath(import.meta.url)));

import * as lock_json from "../web/package-lock.json" with { type: "json" };

let npm_packages = lock_json.default.packages

let ostream = createWriteStream("npm-packages.json")

ostream.write('[')

let first_write = true

for (let package_name in npm_packages) {
	if (!package_name) continue;

	let npm_package = npm_packages[package_name];

	let base64String = npm_package.integrity.substring(7);
	const buffer = Buffer.from(base64String, "base64");
	const sha512 = buffer.toString("hex");

	ostream.write(
		`${first_write ? '' : ','}
	{
		"type": "file",
		"url": "${npm_package.resolved}",
 		"sha512": "${sha512}",
		"dest-filename": "${sha512}.tgz",
		"dest": "npm-cache"
	}
`
	)

	first_write = false
}

ostream.write(']')
ostream.close()