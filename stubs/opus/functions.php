<?php

declare(strict_types=1);


    function safeexport($v) {
        return class_exists(\mixed::class) ? \mixed::class : \stdClass::class;
    }


    function writestubfile($namespace, $className, $code) {
        return class_exists(\mixed::class) ? \mixed::class : \stdClass::class;
    }


    function generatefunctionstubs(string $ext) {
        return class_exists(\mixed::class) ? \mixed::class : \stdClass::class;
    }


    function generateextensionconstants(string $ext) {
        return class_exists(\mixed::class) ? \mixed::class : \stdClass::class;
    }


    function generateclassstubs(array $allowFilters) {
        return class_exists(\mixed::class) ? \mixed::class : \stdClass::class;
    }


    function liststubfolders($dir = '/home/lotus/PROJETOS/opus/stubs') {
        return class_exists(\mixed::class) ? \mixed::class : \stdClass::class;
    }

