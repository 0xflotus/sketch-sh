Modules.require("./Editor_Links.css");

module GetLink = [%graphql
  {|
      query getNote (
        $noteId: String!
      ) {
        note: note (where: {id : {_eq: $noteId}}) {
          ...GqlFragment.Editor.EditorNote
        }
      }
    |}
];

module GetLinkComponent = ReasonApollo.CreateQuery(GetLink);

open Utils;
open Editor_Types;

type uiLink = (string, string);

type link =
  | ReadyLink(Link.link)
  | UiLink(uiLink);

type linkStatus =
  | NotAsked
  | Error
  | Loading
  | Fetched;

type action =
  | Link_Add(uiLink)
  | Link_Fetched(Link.link)
  | Link_Update(Link.link)
  | Link_Delete;

type state = {
  links: array(Link.link),
  fetchingLink: option((string, string)),
};

let singleLink = (name, id) =>
  <div className="link__container">
    <input className="link__input" defaultValue=id placeholder="id" />
    <input className="link__input" defaultValue=name placeholder="name" />
    <button className="link__button link__button--danger">
      <Fi.Trash2 />
    </button>
  </div>;

module EmptyLink = {
  type state = {
    name: string,
    id: string,
  };

  type action =
    | UpdateId(string)
    | UpdateName(string);

  let component = ReasonReact.reducerComponent("Editor_EmptyLink");
  let make = (~status, ~onSubmit, ~onFetched=?, ~name="", ~id="", _children) => {
    ...component,
    initialState: () => {name, id},
    didMount: _ =>
      switch (onFetched) {
      | None => ()
      | Some(f) => f()
      },
    reducer: (action, state) =>
      switch (action) {
      | UpdateId(id) => ReasonReact.Update({...state, id})
      | UpdateName(name) => ReasonReact.Update({...state, name})
      },
    render: ({send, state}) =>
      <div>
        (status == Loading ? "loading"->str : ReasonReact.null)
        <input
          className="link__input"
          value=state.id
          placeholder="id"
          onChange=(id => send(UpdateId(valueFromEvent(id))))
        />
        <input
          className="link__input"
          value=state.name
          placeholder="name"
          onChange=(name => send(UpdateName(valueFromEvent(name))))
        />
        <button
          className="link__button"
          onClick=(_ => onSubmit((state.name, state.id)))
          disabled=(status == Loading)>
          <Fi.Plus />
        </button>
      </div>,
  };
};

let component = ReasonReact.reducerComponent("Editor_Links");

let make = (~links, ~onUpdate, _children) => {
  ...component,
  initialState: () => {links, fetchingLink: None},
  reducer: (action: action, state: state) =>
    switch (action) {
    | Link_Add((name, id)) =>
      ReasonReact.Update({...state, fetchingLink: Some((name, id))})
    | Link_Fetched(link) =>
      ReasonReact.UpdateWithSideEffects(
        {...state, fetchingLink: None},
        (
          _ => {
            let links = Belt.Array.concat(state.links, [|link|]);

            links->Belt.Array.length->string_of_int->Js.log;
            onUpdate(links);
          }
        ),
      )
    | _ => ReasonReact.NoUpdate
    },
  render: ({send, state}) => {
    let existingLinks =
      links
      ->Belt.Array.mapU(
          (. link: Link.link) =>
            switch (link) {
            | Internal({sketch_id, name}) => singleLink(name, sketch_id)
            | External () => ReasonReact.null
            },
        );
    existingLinks->Belt.Array.length->string_of_int->Js.log;

    <div className="links__container">
      <span className="links__disclaimer">
        "Add a link to another sketch by pasting it's id and giving it a module name"
        ->str
      </span>
      <div> existingLinks->ReasonReact.array </div>
      (
        switch (state.fetchingLink) {
        | None =>
          <EmptyLink
            status=NotAsked
            onSubmit=(uiLink => send(Link_Add(uiLink)))
          />
        | Some((name, id)) =>
          let getLinkQuery = GetLink.make(~noteId=id, ());
          <GetLinkComponent variables=getLinkQuery##variables>
            ...(
                 ({result}) =>
                   switch (result) {
                   | Loading =>
                     <EmptyLink
                       status=Loading
                       id
                       name
                       onSubmit=(uiLink => send(Link_Add(uiLink)))
                     />
                   | Error(_error) =>
                     <EmptyLink
                       status=Error
                       id
                       name
                       onSubmit=(uiLink => send(Link_Add(uiLink)))
                     />
                   | Data(response) =>
                     let note = response##note[0];

                     /* TODO handle links that also have links */
                     let (lang, _links, blocks) =
                       switch (note##data) {
                       | None => (Editor_Types.RE, [||], [||])
                       | Some(data) => Editor_Json.V1.decode(data)
                       };

                     let link: Link.link =
                       Internal({
                         revision_at: note##updated_at,
                         sketch_id: note##id,
                         name,
                         lang,
                         code:
                           Editor_Blocks_Utils.concatCodeBlocksToString(
                             blocks,
                           ),
                       });

                     <EmptyLink
                       status=Fetched
                       id
                       name
                       onSubmit=(uiLink => send(Link_Add(uiLink)))
                       onFetched=(() => send(Link_Fetched(link)))
                     />;
                   }
               )
          </GetLinkComponent>;
        }
      )
    </div>;
  },
};