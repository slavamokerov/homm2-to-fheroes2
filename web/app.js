// Front end of the web converter: file selection, immediate batch
// conversion via the WASM module, per-file downloads and a zip archive of
// all results. Runs entirely in the browser — nothing is uploaded anywhere.

"use strict";

( async () => {
    // --- element shortcuts ---
    const $ = ( id ) => document.getElementById( id );
    const fileInput = $( "file-input" );
    const btnClear = $( "btn-clear" );
    const btnDownloadAll = $( "btn-download-all" );
    const downloadAllRow = $( "download-all-row" );
    const downloadAllNote = $( "download-all-note" );
    const dropzone = $( "dropzone" );
    const statusEl = $( "status" );
    const resultsEl = $( "results" );
    const formatSelect = $( "format-select" );

    // --- theme toggle ---
    const themeToggle = $( "theme-toggle" );
    const themeToggleLabel = $( "theme-toggle-label" );
    const applyTheme = ( dark ) => {
        document.documentElement.setAttribute( "data-theme", dark ? "dark" : "light" );
        themeToggleLabel.textContent = dark ? "☀️ Light Mode" : "🌙 Dark Mode";
        themeToggle.setAttribute( "aria-pressed", String( dark ) );
        try { localStorage.setItem( "h2conv-theme", dark ? "dark" : "light" ); } catch ( e ) { /* ignore */ }
    };
    themeToggle.addEventListener( "click", () => {
        applyTheme( document.documentElement.getAttribute( "data-theme" ) !== "dark" );
    } );
    try {
        const saved = localStorage.getItem( "h2conv-theme" );
        applyTheme( saved !== null && saved === "dark" );
    } catch ( e ) {
        applyTheme( false );
    }

    // --- converter state ---
    /** @type {{file: File, info: any, bytes: Uint8Array | null, error: string | null}[]} */
    let entries = [];
    /** @type {any} */
    let Module;
    let converting = false;

    const setStatus = ( text, kind ) => {
        statusEl.textContent = text;
        statusEl.className = "status" + ( kind ? " " + kind : "" );
    };

    const successCount = () => entries.filter( ( e ) => e.bytes !== null ).length;

    const updateButtons = () => {
        const hasAny = entries.length > 0;
        btnClear.classList.toggle( "hidden", !hasAny );
        const ok = successCount() >= 2;
        downloadAllRow.classList.toggle( "hidden", !ok );
        if ( ok )
            downloadAllNote.textContent = successCount() + " of " + entries.length + " converted";
        else
            downloadAllNote.textContent = "";
    };

    const outputName = ( fileName, ext ) => {
        const dot = fileName.lastIndexOf( "." );
        const base = dot > 0 ? fileName.slice( 0, dot ) : fileName;
        return base + ext;
    };

    const renderResults = () => {
        resultsEl.textContent = "";
        for ( const entry of entries ) {
            const row = document.createElement( "div" );
            row.className = "result-row";

            const name = document.createElement( "span" );
            name.className = "result-name";
            name.textContent = entry.file.name;
            row.appendChild( name );

            if ( entry.error !== null ) {
                const err = document.createElement( "span" );
                err.className = "result-error";
                err.textContent = entry.error;
                row.appendChild( err );
            }
            else if ( entry.bytes === null ) {
                const pending = document.createElement( "span" );
                pending.className = "result-info";
                pending.textContent = "Converting…";
                row.appendChild( pending );
            }
            else {
                const info = document.createElement( "span" );
                info.className = "result-info";
                const i = entry.info;
                const parts = [];
                if ( i.slotName && i.slotName !== entry.file.name )
                    parts.push( i.slotName );
                parts.push( ( i.mapName || "?" ) + " — " + i.width + "×" + i.height );
                parts.push( "day " + i.day + ( i.month > 1 ? ", month " + i.month : "" ) );
                parts.push( i.players + " players" );
                parts.push( i.difficultyName || ( "difficulty " + i.difficulty ) );
                info.textContent = parts.join( " · " ) + ( i.campaign ? " · campaign" : "" ) + " → ." + i.outExt.slice( 1 );
                row.appendChild( info );

                const dl = document.createElement( "button" );
                dl.className = "btn btn-primary";
                dl.textContent = "Download";
                dl.addEventListener( "click", () => downloadOne( entry ) );
                row.appendChild( dl );
            }

            resultsEl.appendChild( row );
        }
    };

    const downloadBlob = ( blob, fileName ) => {
        const url = URL.createObjectURL( blob );
        const a = document.createElement( "a" );
        a.href = url;
        a.download = fileName;
        document.body.appendChild( a );
        a.click();
        a.remove();
        setTimeout( () => URL.revokeObjectURL( url ), 2000 );
    };

    const downloadOne = ( entry ) => {
        if ( entry.bytes === null )
            return;
        const ext = entry.info && entry.info.outExt ? entry.info.outExt : ".sav";
        downloadBlob( new Blob( [ entry.bytes ] ), outputName( entry.file.name, ext ) );
    };

    // --- mini zip writer (STORED entries, no compression) ---
    const crcTable = ( () => {
        const table = new Uint32Array( 256 );
        for ( let n = 0; n < 256; ++n ) {
            let c = n;
            for ( let k = 0; k < 8; ++k )
                c = ( c & 1 ) ? ( 0xEDB88320 ^ ( c >>> 1 ) ) : ( c >>> 1 );
            table[n] = c >>> 0;
        }
        return table;
    } )();

    const crc32 = ( bytes ) => {
        let c = 0xFFFFFFFF;
        for ( let i = 0; i < bytes.length; ++i )
            c = crcTable[( c ^ bytes[i] ) & 0xFF] ^ ( c >>> 8 );
        return ( c ^ 0xFFFFFFFF ) >>> 0;
    };

    const buildZip = ( items ) => {
        const encoder = new TextEncoder();
        const chunks = [];
        const central = [];
        let offset = 0;

        for ( const { name, bytes } of items ) {
            const nameBytes = encoder.encode( name );
            const crc = crc32( bytes );

            const local = new Uint8Array( 30 + nameBytes.length );
            const lv = new DataView( local.buffer );
            lv.setUint32( 0, 0x04034b50, true );
            lv.setUint16( 4, 20, true );
            lv.setUint16( 6, 0x0800, true );      // UTF-8 names
            lv.setUint16( 8, 0, true );           // STORED
            lv.setUint16( 10, 0, true );
            lv.setUint16( 12, 0x21, true );
            lv.setUint32( 14, crc, true );
            lv.setUint32( 18, bytes.length, true );
            lv.setUint32( 22, bytes.length, true );
            lv.setUint16( 26, nameBytes.length, true );
            lv.setUint16( 28, 0, true );
            local.set( nameBytes, 30 );

            const centralEntry = new Uint8Array( 46 + nameBytes.length );
            const cv = new DataView( centralEntry.buffer );
            cv.setUint32( 0, 0x02014b50, true );
            cv.setUint16( 4, 20, true );
            cv.setUint16( 6, 20, true );
            cv.setUint16( 8, 0x0800, true );
            cv.setUint16( 10, 0, true );
            cv.setUint16( 12, 0, true );
            cv.setUint16( 14, 0x21, true );
            cv.setUint32( 16, crc, true );
            cv.setUint32( 20, bytes.length, true );
            cv.setUint32( 24, bytes.length, true );
            cv.setUint16( 28, nameBytes.length, true );
            cv.setUint32( 42, offset, true );
            centralEntry.set( nameBytes, 46 );
            central.push( centralEntry );

            chunks.push( local, bytes );
            offset += local.length + bytes.length;
        }

        const centralSize = central.reduce( ( s, c ) => s + c.length, 0 );
        const eocd = new Uint8Array( 22 );
        const ev = new DataView( eocd.buffer );
        ev.setUint32( 0, 0x06054b50, true );
        ev.setUint16( 8, central.length, true );
        ev.setUint16( 10, central.length, true );
        ev.setUint32( 12, centralSize, true );
        ev.setUint32( 16, offset, true );

        chunks.push( ...central, eocd );
        return new Blob( chunks, { type: "application/zip" } );
    };

    const downloadAll = () => {
        const done = entries.filter( ( e ) => e.bytes !== null );
        if ( done.length < 2 )
            return;
        const items = done.map( ( e ) => ( {
            name: outputName( e.file.name, e.info && e.info.outExt ? e.info.outExt : ".sav" ),
            bytes: e.bytes,
        } ) );
        downloadBlob( buildZip( items ), "homm2-converted.zip" );
    };

    // --- file intake: accepted files are converted right away ---
    const ACCEPTED = ( name ) => {
        const n = name.toLowerCase();
        return n.endsWith( ".gm1" ) || n.endsWith( ".gmc" ) || n.endsWith( ".gxc" );
    };

    const convertEntry = async ( entry ) => {
        if ( entry.bytes !== null || entry.error !== null )
            return;
        try {
            const buffer = await entry.file.arrayBuffer();
            const bytes = new Uint8Array( buffer );
            const version = parseInt( formatSelect.value, 10 );
            const json = Module.convertSave( entry.file.name, bytes, version );
            const info = JSON.parse( json );
            if ( info.ok ) {
                entry.info = info;
                entry.bytes = Module.takeLastResult();
            }
            else {
                entry.error = info.error || "Conversion failed.";
            }
        }
        catch ( err ) {
            entry.error = String( err && err.message ? err.message : err );
        }
    };

    const convertAll = async () => {
        if ( typeof Module === "undefined" ) {
            setStatus( "The converter is still loading or failed to load.", "error" );
            return;
        }
        converting = true;
        for ( const entry of entries )
            await convertEntry( entry );
        converting = false;

        setStatus( "", "" );

        updateButtons();
        renderResults();
    };

    const addFiles = ( fileList ) => {
        const accepted = Array.from( fileList ).filter( ( f ) => ACCEPTED( f.name ) );
        if ( accepted.length !== fileList.length )
            setStatus( "Skipped files that are not .GM1 / .GMC / .GXC.", "error" );
        for ( const file of accepted ) {
            if ( entries.some( ( e ) => e.file.name === file.name && e.file.size === file.size ) )
                continue;
            entries.push( { file, info: null, bytes: null, error: null } );
        }
        updateButtons();
        renderResults();
        if ( accepted.length > 0 && !converting )
            convertAll();
    };

    fileInput.addEventListener( "change", () => {
        addFiles( fileInput.files );
        fileInput.value = "";
    } );

    dropzone.addEventListener( "click", () => fileInput.click() );
    dropzone.addEventListener( "dragover", ( e ) => {
        e.preventDefault();
        dropzone.classList.add( "dragover" );
    } );
    dropzone.addEventListener( "dragleave", () => dropzone.classList.remove( "dragover" ) );
    dropzone.addEventListener( "drop", ( e ) => {
        e.preventDefault();
        dropzone.classList.remove( "dragover" );
        addFiles( e.dataTransfer.files );
    } );

    btnDownloadAll.addEventListener( "click", downloadAll );

    btnClear.addEventListener( "click", () => {
        entries = [];
        statusEl.textContent = "";
        statusEl.className = "status";
        updateButtons();
        renderResults();
    } );

    // A different format version re-converts everything already loaded.
    formatSelect.addEventListener( "change", () => {
        for ( const entry of entries ) {
            entry.bytes = null;
            entry.error = null;
            entry.info = null;
        }
        renderResults();
        if ( entries.length > 0 )
            convertAll();
    } );

    // --- module load ---
    try {
        Module = await createH2convModule();
    }
    catch ( err ) {
        setStatus( "The converter failed to load. Open this page over http(s), not as a local file.", "error" );
        return;
    }

    setStatus( "", "" );
} )();
