'use strict';
(() => {
  const $ = id => document.getElementById(id);
  const data = window.VELOGRAPH_REPORT;
  const sourceBase = 'https://github.com/SpeziMM/VeloGraph/blob/main/';
  const svgNS = 'http://www.w3.org/2000/svg';
  const svg = (tag, attrs, parent, text) => {
    const element = document.createElementNS(svgNS, tag);
    for (const [key, value] of Object.entries(attrs)) element.setAttribute(key, String(value));
    if (text !== undefined) element.textContent = text;
    parent.appendChild(element);
    return element;
  };
  const label = (parent, x, y, text, extra = {}) => svg('text', {x,y,fill:'#172d35','font-size':14,...extra},parent,text);
  const stages = [
    {title:'Load & index', description:'Stream OSM ways into a directed graph, merge compatible degree-2 nodes and build a spatial index. A binary cache avoids reparsing an unchanged input.', structure:'Hash map of node IDs → nodes; per-node adjacency vectors and incoming vectors; KD-tree stored as an ID vector.', efficiency:'Expected O(1) ID lookup; contiguous edges within an adjacency vector. KD-tree pruning avoids scanning every node on typical nearest/radius queries.', gap:'Hash maps still scatter data in memory. Index construction is serial; simplification discards intermediate geometry. OSM access and one-way interpretation is incomplete.', file:'src/Graph.cpp',symbol:'Graph::buildSpatialIndex / simplifyGraph',caption:'Gray arrows are directed graph edges; points represent retained nodes.'},
    {title:'Select a start',description:'Use an explicit node, snap a coordinate within max_snap, or gather eligible nodes in a radius. Area starts are ordered by distance and ID, then capped.',structure:'KD-tree nearest/radius query → vector of {node ID, offset}. Area candidates are sorted deterministically.',efficiency:'The index prunes spatial queries; only a bounded number of starts launch routing. Sorting M eligible area nodes costs O(M log M).',gap:'Nearest-first starts can cluster. Eligibility does not prove a cycle exists. Radius search is approximate geographic distance; it constrains only the start, not the route.',file:'src/StartSelection.cpp',symbol:'StartSelection::nearest / inArea',caption:'The start is snapped to a graph node. An area may offer several candidate starts.'},
    {title:'Precompute',description:'Collect nearby nodes, compute shortest distances home through incoming edges, and mark low-degree nodes as dead ends for waypoint filtering.',structure:'KD-tree radius query; reverse Dijkstra with a min-heap and distance hash map; hash sets for local nodes and dead ends.',efficiency:'Compute shared return-distance information once per start, then reuse it across I attempts. Bounded Dijkstra is roughly O((V + E) log V) with expected constant-time map operations.',gap:'Precompute is serial. Degree ≤ 1 is a heuristic: a directed ring can have one outgoing edge yet still contain a cycle. Local filters and short-edge exclusions may prune useful options.',file:'src/RoutePrecompute.cpp',symbol:'RoutePrecompute::precompute',caption:'Incoming-edge traversal estimates distance home; it never authorizes riding a one-way edge backwards.'},
    {title:'Place waypoints',description:'Generate 4–6 positions on a jittered ellipse around the start; snap to graph nodes, filter by return reachability, remove duplicates and order by bearing.',structure:'Seeded mt19937 RNG, a short waypoint vector, KD-tree snaps, hash lookups into precompute and a tiny sort.',efficiency:'Only a few waypoints per attempt. Shared spatial/reachability data keeps candidate generation inexpensive relative to route searches.',gap:'The 1.3 road-factor and ellipse shape are heuristics. Snapping and filtering can collapse the template or miss useful corridors. Bearing order does not guarantee a non-self-crossing road route.',file:'src/WaypointGenerator.cpp',symbol:'WaypointGenerator::generate',caption:'Amber points illustrate snapped waypoints; they are not waypoints captured from the recorded route.'},
    {title:'Connect segments',description:'Search between waypoints with fitness-weighted costs and separate meter budgets. When segments fail, use a return search or the greedy outbound-and-return fallback.',structure:'Binary-heap priority queue, cost/parent hash maps, avoid sets and forward adjacency vectors. One cost label per node.',efficiency:'Heuristics prioritize promising frontier entries and explicit budgets bound exploration. Adjacency iteration visits only outgoing neighbors.',gap:'This is not exact constrained A*. One cost label can discard a longer-cost but shorter-distance alternative. Search limits, revisit rules and short-edge filters can miss feasible loops.',file:'src/RouteFinder.cpp',symbol:'findSegmentPath / findReturnPath / buildCircularRoute',caption:'Blue edges illustrate connecting waypoints. Real search expansions are not recorded in this report.'},
    {title:'Refine & correct',description:'Try 2-opt segment reversals, then shortcuts or detours to adjust length. Check connectivity before copying a candidate path.',structure:'Route vectors, adjacency lookups, full route scoring and bounded local-search loops.',efficiency:'Early rejection avoids allocating impossible swaps. Work is bounded, but many pairwise swaps and repeated scoring still make long routes expensive.',gap:'O(k²) pairs per sweep; each surviving trial can require O(k) validation/scoring plus edge scans. Correction uses a fixed 10% trigger; user tolerance only controls final ranking.',file:'src/RouteFinder.cpp',symbol:'improveWith2Opt / correctDistance',caption:'Green edges illustrate the refined candidate. Local improvement is not global optimality.'},
    {title:'Validate & select',description:'Require closure and existing forward edges; reject forbidden road classes. Prefer candidates within tolerance, then maximize fitness plus distance accuracy.',structure:'Independent CPU attempts → result vector → deterministic ordered reduction. Area starts use the same preference rule.',efficiency:'Read-only graph/precompute sharing avoids locks inside attempts. Reduction costs O(I); the caller also works, and a one-worker search avoids launching a thread.',gap:'A bounded search can return nothing or a best-effort loop. Graph validity is not full OSM legality. Fixed-seed repeatability is for the same data/build, not guaranteed across platforms.',file:'src/RouteFinder.cpp',symbol:'runIteration / findOptimalCycle',caption:'The final loop is selected from attempted candidates; the best possible graph loop is not guaranteed.'}
  ];
  const teachingPoints = Array.from({length:20}, (_,i) => [65+(i%5)*125,50+Math.floor(i/5)*82]);
  const teachingPath = [10,11,6,7,8,13,18,17,16,15,10];
  function drawTeaching(stage) {
    const root = $('pipeline-graph'); root.replaceChildren();
    const defs = svg('defs',{},root);
    const arrow = svg('marker',{id:'edge-arrow',markerWidth:6,markerHeight:6,refX:5,refY:3,orient:'auto-start-reverse'},defs);
    svg('path',{d:'M0,0 L6,3 L0,6',fill:'#a8bbb7'},arrow);
    teachingPoints.forEach(([x,y],i) => {
      for (const next of [i%5<4?i+1:-1,i<15?i+5:-1]) if(next>=0) {
        const [nx,ny] = teachingPoints[next];
        const length=Math.hypot(nx-x,ny-y),ux=(nx-x)/length,uy=(ny-y)/length;
        svg('line',{x1:x+7*ux,y1:y+7*uy,x2:nx-7*ux,y2:ny-7*uy,stroke:'#b8c8c4','stroke-width':2,'marker-end':'url(#edge-arrow)','marker-start':'url(#edge-arrow)'},root);
      }
    });
    if(stage===1) svg('circle',{cx:65,cy:214,r:89,fill:'#087866',opacity:.08,stroke:'#087866'},root);
    if(stage===2) teachingPoints.forEach(([x,y],i)=>label(root,x+9,y-10,`${Math.abs(i-10)*70}m`,{'font-size':22,fill:'#52656b'}));
    if(stage>=4) svg('polyline',{points:teachingPath.map(i=>teachingPoints[i].join(',')).join(' '),fill:'none',stroke:stage>=5?'#087866':'#346bb3','stroke-width':5,'stroke-linejoin':'round'},root);
    teachingPoints.forEach(([x,y],i)=>svg('circle',{cx:x,cy:y,r:i===10?10:5,fill:i===10?'#087866':'#fff',stroke:'#68857d','stroke-width':2},root));
    if(stage>=3 && stage<6) [6,8,18,16].forEach(i=>svg('circle',{cx:teachingPoints[i][0],cy:teachingPoints[i][1],r:9,fill:'#e5aa40',stroke:'#ad610a','stroke-width':2},root));
    label(root,42,245,'start',{'font-size':22,'font-weight':700});
  }
  function chooseStage(index) {
    const stage=stages[index];
    $('stage-number').textContent=`Stage ${index+1} / ${stages.length}`;
    $('stage-title').textContent=stage.title;
    $('stage-description').textContent=stage.description;
    $('stage-data').textContent=stage.structure;
    $('stage-efficiency').textContent=stage.efficiency;
    $('stage-gap').textContent=stage.gap;
    $('stage-source').href=sourceBase+stage.file;
    $('stage-source').textContent=stage.symbol+' ↗';
    $('graph-caption').textContent=stage.caption;
    [...$('stage-buttons').children].forEach((button,i)=>button.setAttribute('aria-pressed',i===index));
    drawTeaching(index);
  }
  stages.forEach((stage,i)=>{
    const button=document.createElement('button');button.type='button';button.textContent=`${i+1}. ${stage.title}`;
    button.addEventListener('click',()=>chooseStage(i));$('stage-buttons').appendChild(button);
  });
  chooseStage(0);
  const quote = value => "'"+value.replaceAll("'", "'\\''")+"'";
  function request() {
    const mode=$('start-mode').value;
    document.querySelectorAll('[data-coordinate]').forEach(el=>{el.hidden=mode==='node';el.querySelector('input').disabled=mode==='node';});
    for(const [field,show] of [['node-field',mode==='node'],['snap-field',mode==='point'],['radius-field',mode==='area'],['candidates-field',mode==='area']]) {
      $(field).hidden=!show;$(field).querySelector('input').disabled=!show;
    }
    const diagram=$('start-diagram');diagram.replaceChildren();
    svg('rect',{x:10,y:10,width:340,height:210,rx:10,fill:'#edf3f0'},diagram);
    svg('circle',{cx:180,cy:110,r:mode==='area'?82:mode==='point'?40:0,fill:'#cde6db',stroke:'#087866','stroke-dasharray':'5 4'},diagram);
    [[160,95],[190,135],[245,85],[115,155],[265,175]].forEach(([x,y],i)=>svg('circle',{cx:x,cy:y,r:6,fill:i===0?'#087866':'#97b3ac'},diagram));
    label(diagram,172,113,'×',{'font-size':22});label(diagram,25,204,mode==='area'?'Center + bounded candidate starts':mode==='point'?'Coordinate + maximum snap':'One explicit graph node',{'font-size':13});
    $('node-id').setCustomValidity(mode==='node' && (!/^[0-9]+$/.test($('node-id').value) || BigInt($('node-id').value)>9223372036854775807n) ? 'Use a nonnegative signed 64-bit node ID.' : '');
    if(!$('request-form').checkValidity()) { $('command').textContent='Enter valid values in all active fields.';$('request-summary').textContent='Request needs correction.';$('copy-command').disabled=true;return; }
    const args=['./build/quality/VeloGraph',quote($('map-path').value)];
    if(mode==='node') args.push('--start_node',$('node-id').value);
    else {
      args.push('--start',$('latitude').value,$('longitude').value);
      if(mode==='area') args.push('--start_radius',$('radius').value,'--start_candidates',String(Number($('candidates').value)));
      else args.push('--max_snap',$('snap').value);
    }
    args.push('--target_distance',$('distance').value,'--tolerance',String(Number($('tolerance').value)/100),'--profile',$('profile').value,'--iterations',String(Number($('iterations').value)),'--threads',String(Number($('threads').value)),'--seed',String(Number($('seed').value)),'--engine',$('engine').value,'--output_path','output/route.json');
    $('command').textContent=args.join(' ');$('copy-command').disabled=false;
    const target=Number($('distance').value),tolerance=Number($('tolerance').value)/100;
    const attempts=Number($('iterations').value)*(mode==='area'?Number($('candidates').value):1);
    $('request-summary').textContent=`Preferred distance band: ${(target*(1-tolerance)).toFixed(0)}–${(target*(1+tolerance)).toFixed(0)} m. Up to ${attempts.toLocaleString()} attempts${mode==='area'?' across the nearest eligible starts':''}. Finish equals the chosen start. Best effort may fall outside this band.`;
    $('copy-status').textContent='';
  }
  $('request-form').addEventListener('submit',event=>event.preventDefault());
  $('request-form').addEventListener('input',request);$('request-form').addEventListener('change',request);
  $('copy-command').addEventListener('click',async()=>{
    try {await navigator.clipboard.writeText($('command').textContent);$('copy-status').textContent='Copied.';}
    catch { $('copy-status').textContent='Select the command above and copy it.'; }
  });
  const exampleStart=data.routes.point.nodes[0];
  $('latitude').value=exampleStart.lat;$('longitude').value=exampleStart.lon;$('node-id').value=exampleStart.id;
  request();
  data.cli_help.forEach(line=>{const item=document.createElement('li');const code=document.createElement('code');code.textContent=line;item.appendChild(code);$('cli-inputs').appendChild(item);});
  let currentRoute;
  function validateRoute(route) {
    if(!route || !Array.isArray(route.nodes) || route.nodes.length>10000) throw Error('Expected nodes array with at most 10,000 points.');
    for(const point of route.nodes) {
      if(!point || !Number.isFinite(point.lat)||!Number.isFinite(point.lon)||Math.abs(point.lat)>90||Math.abs(point.lon)>180||!(typeof point.id==='string'||Number.isSafeInteger(point.id))) throw Error('Each point needs a valid id, latitude and longitude.');
    }
    return route;
  }
  function loadRoute(route, origin) {
    currentRoute=validateRoute(route);$('route-error').textContent='';
    $('route-position').max=Math.max(0,route.nodes.length-1);$('route-position').value=0;
    $('route-position').disabled=!route.nodes.length;
    const run=route.run||{},stats=route.stats||{};
    const distance=Number.isFinite(stats.total_distance_m)?`${(stats.total_distance_m/1000).toFixed(2)} km`:'distance unavailable';
    $('route-summary').textContent=`${origin}: ${route.nodes.length} points · ${distance} · ${!route.nodes.length?'no route found':run.within_tolerance===true?'within target tolerance':'best effort / tolerance unconfirmed'}.`;
    $('route-meta').textContent=`Seed ${run.seed??'unknown'} · ${run.engine??'unknown'} engine · start mode ${run.start_mode??'node (legacy metadata)'}. ${run.searched_starts!==undefined?`${run.searched_starts} searched of ${run.eligible_starts} eligible starts.`:''} Imported files are displayed, not independently graph-validated.`;
    renderRoute();
  }
  function renderRoute() {
    const root=$('real-route');root.replaceChildren();const points=currentRoute.nodes;
    if(!points.length){label(root,40,180,'No route found. Feasibility remains unknown.');$('point-json').textContent='[]';$('point-position').textContent='No points';$('route-prev').disabled=true;$('route-next').disabled=true;return;}
    const index=Number($('route-position').value),lat0=points.reduce((sum,p)=>sum+p.lat,0)/points.length;
    const xs=points.map(p=>p.lon*Math.cos(lat0*Math.PI/180)),ys=points.map(p=>-p.lat);
    const minX=Math.min(...xs),minY=Math.min(...ys),dx=Math.max(...xs)-minX,dy=Math.max(...ys)-minY;
    const scale=Math.min(540/Math.max(dx,.00001),290/Math.max(dy,.00001));
    const projected=points.map((p,i)=>[50+(xs[i]-minX)*scale,40+(ys[i]-minY)*scale]);
    svg('polyline',{points:projected.map(p=>p.join(',')).join(' '),fill:'none',stroke:'#b0c1bb','stroke-width':5,'stroke-linejoin':'round'},root);
    svg('polyline',{points:projected.slice(0,index+1).map(p=>p.join(',')).join(' '),fill:'none',stroke:'#087866','stroke-width':5,'stroke-linejoin':'round'},root);
    const [sx,sy]=projected[0];svg('circle',{cx:sx,cy:sy,r:8,fill:'#fff',stroke:'#087866','stroke-width':3},root);label(root,sx+12,sy+4,'start / finish',{'font-size':22});
    const [x,y]=projected[index];svg('circle',{cx:x,cy:y,r:7,fill:'#e5aa40',stroke:'#172d35','stroke-width':2},root);
    $('point-position').textContent=`Point ${index+1} of ${points.length} · retained geometry`;
    $('point-json').textContent=JSON.stringify({point_index:index,...points[index]},null,2);
    $('route-prev').disabled=index===0;$('route-next').disabled=index===points.length-1;
  }
  $('route-example').addEventListener('change',()=>loadRoute(data.routes[$('route-example').value],'Recorded engine output'));
  $('route-position').addEventListener('input',renderRoute);
  for(const [button,delta] of [['route-prev',-1],['route-next',1]]) $(button).addEventListener('click',()=>{$('route-position').value=Number($('route-position').value)+delta;renderRoute();});
  $('route-file').addEventListener('change',async()=>{
    const file=$('route-file').files[0];if(!file)return;
    try {if(file.size>3000000)throw Error('File exceeds 3 MB limit.');const route=JSON.parse(await file.text());validateRoute(route);loadRoute(route,'Local file');}
    catch(error){$('route-error').textContent=`Could not load route: ${error.message}`;}
  });
  loadRoute(data.routes.point,'Recorded engine output');
  const failures={
    input:['Rejected request','Non-finite values, conflicting start modes or coordinates beyond the snapping limit are rejected before search. An empty area means no eligible start was selected under that query.','Fix the input, inspect the dataset coverage or change the explicit radius/snap limit. This is not a general route impossibility result.'],
    directed:['No result on a one-way chain','The regression fixture has no return edge and the engine correctly emits no loop. The current runtime does not produce a general proof or detailed connectivity diagnosis.','Inspect forward connectivity. Formal reason codes and sound necessary-condition checks are future work in #5.'],
    budget:['Search exhausted; feasibility unknown','Waypoint filtering, one cost label per node, expansion limits and finite attempts can miss an existing suitable loop.','More iterations or different seeds may help but do not guarantee success. Never relabel exhaustion as “impossible.”'],
    tolerance:['Valid best effort','A graph-valid loop can be returned outside the requested distance band. The tolerance is a ranking preference.','Read run.within_tolerance separately from run.success. Decide whether to accept the route or change the request.'],
    area:['Only a bounded subset of starts was tried','Nearest-first candidate selection may omit a start with a better loop. eligible_starts and searched_starts expose this boundary.','Increase --start_candidates up to 64 or move the center. Failure across the selected subset does not rule out the entire area.'],
    tags:['Graph-consistent is not fully OSM-compliant','The parser does not yet cover all bicycle access restrictions, oneway=-1, roundabouts or bicycle-specific exceptions. A legal graph edge may not reflect real access.','Treat this as an experimental router. Correct parser semantics and add representative OSM fixtures before making full restriction-compliance claims.'],
    geometry:['Retained points are not the full road shape','Degree-2 simplification retains accumulated distance but discards intermediate road geometry. Straight lines between exported points can cut corners.','Distance comes from graph edge weights; the polyline is an approximation. Preserving edge geometry is separate future work.']
  };
  function showFailure(){const [title,explanation,action]=failures[$('failure-case').value];$('failure-title').textContent=title;$('failure-explanation').textContent=explanation;$('failure-action').textContent=action;}
  $('failure-case').addEventListener('change',showFailure);showFailure();
  function benchmark(){
    const rows=data.benchmarks[$('benchmark-set').value],timing=$('benchmark-metric').value==='time';
    const root=$('benchmark-chart');root.replaceChildren();$('benchmark-table').replaceChildren();
    const max=timing?Math.max(...rows.map(r=>r.p90_ms))*1.15:rows[0].cases;
    label(root,175,25,timing?'Milliseconds per search · lower is faster':'Cases · graph-valid and within tolerance',{'font-size':14});
    for(let i=0;i<=4;i++){const x=175+i*125;svg('line',{x1:x,y1:42,x2:x,y2:218,stroke:'#d5e0df'},root);label(root,x,246,(max*i/4).toFixed(timing?1:0),{'font-size':12,'text-anchor':'middle'});}
    rows.forEach((r,i)=>{
      const y=65+i*88;label(root,10,y+22,r.engine,{'font-weight':700});
      const outer=timing?r.p90_ms:r.valid,inner=timing?r.median_ms:r.in_tolerance;
      svg('rect',{x:175,y,width:500*outer/max,height:22,rx:3,fill:'#b9d1e6'},root);
      svg('rect',{x:175,y:y+27,width:500*inner/max,height:22,rx:3,fill:'#087866'},root);
      label(root,185+500*outer/max,y+16,timing?`${outer.toFixed(2)} p90`:`${outer} valid`,{'font-size':12});
      label(root,185+500*inner/max,y+43,timing?`${inner.toFixed(2)} median`:`${inner} in band`,{'font-size':12});
      const tr=document.createElement('tr');
      [r.engine,r.cases,r.valid,r.in_tolerance,r.mean_fitness.toFixed(4),r.mean_error_m.toFixed(1),`${r.median_ms.toFixed(3)} / ${r.p90_ms.toFixed(3)}`].forEach(value=>{const td=document.createElement('td');td.textContent=value;tr.appendChild(td);});$('benchmark-table').appendChild(tr);
    });
    $('benchmark-note').textContent=$('benchmark-set').value==='curated'?'Selection-biased: these starts were curated from earlier reported successes. Three timed repeats per case.':'Held-out starts were not filtered by route success after sampling. Three timed repeats per case; still only one region and target length.';
  }
  $('benchmark-set').addEventListener('change',benchmark);$('benchmark-metric').addEventListener('change',benchmark);benchmark();
  $('provenance').textContent=`Reviewed ${data.review.date} · source fingerprint ${data.review.source_digest.slice(0,12)} · evidence is versioned, not live telemetry.`;
  $('review-note').textContent=`Review note: ${data.review.note}`;
})();
